#include "disassemblyview.hpp"

#include <iterator>

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QPainter>
#include <QScrollBar>
#include <QRegularExpression>
#include <QPlainTextEdit>
#include <QHBoxLayout>

#include "vm/runner.hpp"
#include "core.hpp"
#include "memory.hpp"
#include "opcode.hpp"
#include "utilities.hpp"

namespace
{

class DisassemblyTextEdit : public QPlainTextEdit
{
    Q_OBJECT

public:
    using QPlainTextEdit::QPlainTextEdit;

    // Expose protected as public for BreakpointSidebar
    QTextBlock firstVisibleBlock() const { return QPlainTextEdit::firstVisibleBlock(); }
    QPointF contentOffset() const { return QPlainTextEdit::contentOffset(); }
    QRectF blockBoundingGeometry(const QTextBlock& block) const { return QPlainTextEdit::blockBoundingGeometry(block); }
    QRectF blockBoundingRect(const QTextBlock& block) const { return QPlainTextEdit::blockBoundingRect(block); }
};

// Sidebar to add and remove breakpoint from user interactions
class BreakpointSidebar : public QWidget
{
    Q_OBJECT

public:
    explicit BreakpointSidebar(DisassemblyTextEdit* editor)
        : QWidget{editor}
        , m_editor{editor}
    {
        m_fontHeight = QFontMetrics{m_editor->font()}.height();

        setMouseTracking(true);

        // Update sidebar geometry when editor geometry changes
        connect(m_editor, &QPlainTextEdit::updateRequest, this, qOverload<>(&QWidget::update));
        connect(m_editor->document(), &QTextDocument::blockCountChanged, this, &BreakpointSidebar::updateGeometry);
        connect(m_editor->verticalScrollBar(), &QScrollBar::valueChanged, this, qOverload<>(&QWidget::update));

        setMinimumWidth(m_fontHeight + 4);
        updateGeometry();
    }

    QSize sizeHint() const override
    {
        return QSize{m_fontHeight + 4, 0}; // width is fixed
    }

    void setRunner(VMRunner& runner)
    {
        if(m_runner) // disconned old runner in case it is still used elsewhere
        {
            m_runner->disconnect(this);
        }

        m_runner = &runner;
        connect(m_runner, &VMRunner::statusChanged, this, [this](VMRunner::Status status)
        {
            if(status == VMRunner::Status::Stopped)
            {
                m_wasStopped = true;
            }
            else if(m_wasStopped) // restore breakpoints
            {
                for(auto&& [address, state] : m_breakpoints)
                {
                    m_runner->addBreakpoint(address, state.enabled);
                }

                m_wasStopped = false;
            }
        });
    }

    void setLineToAddress(const std::vector<uint64_t>& info)
    {
        m_lineToAddress = &info;
    }

signals:
    void breakpointToggled(int line, bool enabled);

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QPainter painter{this};
        painter.setRenderHint(QPainter::Antialiasing);

        QTextBlock block = m_editor->firstVisibleBlock();

        // First block may be partially scrolled, when at the top of the 
        const auto firstBlockRect = m_editor->blockBoundingGeometry(block);
        int top = qRound(firstBlockRect.translated(m_editor->contentOffset()).top());
        int bottom = top + qRound(firstBlockRect.height());

        int blockNumber = block.blockNumber();
        while(block.isValid() && blockNumber < m_lineToAddress->size() && top <= event->rect().bottom())
        {
            if(block.isVisible() && bottom >= event->rect().top())
            {
                const int iconSize = std::min(m_fontHeight - 2, bottom - top - 1);
                const QRect iconRect{1, top + (bottom - top - iconSize) / 2, iconSize, iconSize};

                const auto address = (*m_lineToAddress)[blockNumber];
                const auto bp = m_breakpoints.find(address);
                if(bp != m_breakpoints.end())
                {
                    if(bp->second.enabled)
                    {
                        painter.setPen(Qt::red);
                        painter.setBrush(Qt::red);
                    }
                    else
                    {
                        painter.setPen(Qt::gray);
                        painter.setBrush(Qt::lightGray);
                    }

                    painter.drawEllipse(iconRect);
                }
                else if(m_breakpointPreview == blockNumber)
                {
                    painter.setPen(Qt::darkGray);
                    painter.setBrush(Qt::black);
                    painter.drawEllipse(iconRect);
                }
            }

            block = block.next();
            top = bottom;
            bottom = top + qRound(m_editor->blockBoundingRect(block).height());
            ++blockNumber;
        }
    }
    
    void leaveEvent(QEvent* event) override
    {
        m_breakpointPreview = -1;
        update();
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        // Map sidebar coordinates to editor viewport
        const int index = m_editor->cursorForPosition(QPoint{0, event->pos().y()}).blockNumber();
        if(m_breakpointPreview != index)
        {
            m_breakpointPreview = index;
            update();
        }
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if(event->button() != Qt::LeftButton)
        {
            return;
        }

        const int blockNumber = m_editor->cursorForPosition(QPoint{0, event->pos().y()}).blockNumber();
        if(blockNumber >= m_lineToAddress->size())
        {
            return; // beyond end of document
        }

        // if it exists remove it, otherwise add it
        const auto address = (*m_lineToAddress)[blockNumber];
        const auto it = m_breakpoints.find(address);
        if(it != m_breakpoints.end())
        {
            m_breakpoints.erase(it);
            m_runner->removeBreakpoint(address);
        }
        else
        {
            m_breakpoints.emplace(address, BreakpointState{true});
            m_runner->addBreakpoint(address, true);
        }

        update();
    }

private:
    DisassemblyTextEdit* m_editor{};
    VMRunner* m_runner{};
    bool m_wasStopped{};

    // This is filled by the disassembly
    const std::vector<uint64_t>* m_lineToAddress{};

    // keep a copy of breakpoints info in the main thread
    struct BreakpointState
    {
        bool enabled{true};
    };
    std::unordered_map<uint64_t, BreakpointState> m_breakpoints{};

    int m_breakpointPreview{-1};
    int m_fontHeight{};
};

// Highlighter class for this text edit. It is a simple non-semantic highlight based on regexes.
class Highlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    Highlighter(QTextDocument* parent = nullptr)
        : QSyntaxHighlighter(parent)
    {
        const auto addRule = [this](const QString& regex, const QColor& color, int weight = QFont::Normal)
        {
            QTextCharFormat format{};
            format.setForeground(color);
            format.setFontWeight(weight);
            m_rules.emplace_back(QRegularExpression(regex), format);
        };

        // comment
        addRule(QStringLiteral(";[^\n]*"), Qt::darkGreen);
        // string
        addRule(QStringLiteral("\"(?:[^\\\\\"]|\\\\.)*\""), Qt::darkRed);
        // label
        addRule(QStringLiteral("\\b\\w*:"), Qt::blue, QFont::Bold);
        // directives
        addRule(QStringLiteral("\\.\\w+"), Qt::darkGreen);
        // control-flow instructions
        addRule(QStringLiteral("\\b(bne|beq|bl|ble|bg|bge|bls|bles|bgs|bges|bra|loop|jump|call|jumpbr|callbr|ret|reti|syscall|int)\\b"), Qt::darkMagenta, QFont::Bold);
        // fixed instructions
        addRule(QStringLiteral("\\b(nop|moveix|ext|ins|movei|moven|umove|move)\\b"), Qt::darkCyan, QFont::Bold);
        // sized instructions
        addRule(QStringLiteral("\\b(add|sub|xor|or|and|lsl|asr|lsr|se|slts|sltu|sand|hadd|hsub|hmul|hto|cmp|test|cmpfr|testfr|cmove|sext|rtl|rtr|adc|sbc|ld|st|ldv|stv|lds|sts|ldvs|stvs|div|divu|mul|mulu)(\\.[bwdq])?\\b"), Qt::darkCyan, QFont::Bold);
        // general purpose registers
        addRule(QStringLiteral("\\b(r|a|t|n|s|S|R|A|T|N|sp|SP)\\d+\\b"), Qt::darkYellow);
        // special registers
        addRule(QStringLiteral("\\b(LR|lr|BR|br|LC|lc|FR|fr|PC|pc|IR|ir|CC|cc|IC|ic|PL|pl|PH|ph|PQ|pq|PR|pr)\\b"), Qt::darkRed);
        // numerics
        addRule(QStringLiteral("\\b(0x|0X|0b|0B|0o|0O|\\d)[\\dA-Fa-f]*(\\.\\d*(f|)|)\\b"), Qt::darkRed);
        // operator
        addRule(QStringLiteral(","), Qt::darkGray);
    }

    void setDebugComment(int line, QString comment)
    {
        clearDebugComment();

        if(comment.isEmpty())
        {
            return;
        }

        QTextBlock block = document()->findBlockByLineNumber(line);
        QTextCursor cursor{block};
        if(!cursor.isNull())
        {
            m_oldLineContent = block.text(); // save old line to restore it later

            cursor.movePosition(QTextCursor::EndOfBlock);
            cursor.insertText(" ; " + comment);
            m_commentedLine = line;
        }
    }

    void clearDebugComment()
    {
        if(m_commentedLine != -1)
        {
            QTextBlock block = document()->findBlockByLineNumber(m_commentedLine);
            QTextCursor cursor{block};
            while(!cursor.atBlockEnd())
            {
                cursor.deleteChar();
            }

            // remove text
            cursor.insertText(m_oldLineContent);
            m_oldLineContent = {};
            m_commentedLine = -1;
        }
    }

protected:
    void highlightBlock(const QString& text) override
    {
        for(const HighlightingRule& rule : std::as_const(m_rules))
        {
            QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
            while(matchIterator.hasNext())
            {
                QRegularExpressionMatch match = matchIterator.next();
                setFormat(match.capturedStart(), match.capturedLength(), rule.format);
            }
        }

        setCurrentBlockState(0);
    }

private:
    struct HighlightingRule
    {
        QRegularExpression pattern;
        QTextCharFormat format;
    };

    std::vector<HighlightingRule> m_rules;
    int m_commentedLine = -1;
    QString m_oldLineContent{};
};

}

struct DisassemblyView::Internals
{
    VMRunner* runner{};
    DisassemblyTextEdit* textEdit{};
    Highlighter* highligher{};
    BreakpointSidebar* sidebar{};
    std::vector<uint64_t> lineToAddress{};

    // -1 if address is out of document, line index otherwise
    int64_t lineFromAddress(uint64_t address)
    {
        const auto it = std::lower_bound(lineToAddress.begin(), lineToAddress.end(), address);
        if(it != lineToAddress.end() && *it == address)
        {
            return std::distance(lineToAddress.begin(), it);
        }

        return -1;
    }
};

DisassemblyView::DisassemblyView(QWidget* parent)
    : QWidget{parent}
    , m_impl{new Internals}
{
    setWindowTitle(tr("DisassemblyView"));

    QFont font;
    font.setFamilies({QString::fromUtf8("Courier New")});
    font.setBold(false);
    m_impl->textEdit = new DisassemblyTextEdit{this};
    m_impl->textEdit->setFont(font);
    m_impl->textEdit->setReadOnly(true);
    m_impl->textEdit->setBackgroundVisible(false);
    m_impl->textEdit->setPlaceholderText(tr("Assembly will show here when paused..."));
    // this is required to ensure one line == one info (label, instruction, ...)
    m_impl->textEdit->setWordWrapMode(QTextOption::WrapMode::NoWrap);

    m_impl->sidebar = new BreakpointSidebar{m_impl->textEdit};
    m_impl->sidebar->setLineToAddress(m_impl->lineToAddress);

    auto* horizontalLayout = new QHBoxLayout{this};
    horizontalLayout->setSpacing(0);
    horizontalLayout->setContentsMargins(2, 2, 2, 2);
    horizontalLayout->addWidget(m_impl->sidebar);
    horizontalLayout->addWidget(m_impl->textEdit);

    m_impl->highligher = new Highlighter{m_impl->textEdit->document()};
}

void DisassemblyView::setRunner(VMRunner& runner)
{
    if(m_impl->runner) // disconned old runner in case it is still used elsewhere
    {
        m_impl->runner->disconnect(this);
    }

    m_impl->runner = &runner;
    m_impl->sidebar->setRunner(runner);

    connect(m_impl->runner, &VMRunner::statusChanged, this, &DisassemblyView::onStatusChanged);

    disassemble(); // TODO: make this lazy
}

void DisassemblyView::disassemble()
{
    const AxCore* core = m_impl->runner->core();
    if(!core)
    {
        return;
    }

    m_impl->textEdit->clear();
    m_impl->lineToAddress.clear();

    AxPrettyFormatter formatter{*core};
    const uint32_t* wram = reinterpret_cast<const uint32_t*>(core->memory().map(*core, AxMemory::WRAM_BEGIN));

    uint64_t lastAddress{};
    // Disassemble program
    for(const AxCore::Symbol& symbol : core->symbols())
    {
        if(symbol.address == lastAddress)
        {
            continue; // skip aliases
        }

        lastAddress = symbol.address;

        uint64_t currentOffset{};
        while(currentOffset < symbol.size)
        {
            const uint64_t programAddress = symbol.address + currentOffset;
            const uint64_t pc = programAddress / 4ull;
            const uint64_t wramAddress = AxCore::pc_to_wram(pc);

            auto [first, second] = AxOpcode::analyze(wram[pc], wram[pc + 1u]);
            if(!first.valid())
            {
                break; // failed to decode, continue to next symbol
            }

            const auto insertText = [&](const AxOpcodeInfo& info, uint64_t offset)
            {
                formatter.set_base_address(programAddress + offset);
                m_impl->textEdit->appendPlainText(QString{"0x%1\t%2"}.arg(wramAddress + offset, 16, 16, u'0').arg(info.to_string(&formatter)));
                m_impl->lineToAddress.emplace_back(wramAddress + offset);
                currentOffset += 4;
            };

            insertText(first, 0);

            if(second.valid())
            {
                insertText(second, 4);
            }
        }
    }

    for(const auto& [address, name] : formatter.labels())
    {
        const auto realAddress = AxMemory::WRAM_BEGIN + address;

        // Find the line number where this address appears
        auto it = std::lower_bound(m_impl->lineToAddress.begin(), m_impl->lineToAddress.end(), realAddress);
        if(it != m_impl->lineToAddress.end() && *it == realAddress)
        {
            const auto lineNumber = std::distance(m_impl->lineToAddress.begin(), it);

            // Get the block at this line and insert the label before it
            QTextCursor cursor{m_impl->textEdit->document()->findBlockByLineNumber(lineNumber)};
            cursor.movePosition(QTextCursor::StartOfBlock);
            cursor.insertText(QString{"%1:\n"}.arg(name));

            // Update lineToAddress mapping: insert the address for the new label line
            m_impl->lineToAddress.insert(it, realAddress);
        }
    }
}

void DisassemblyView::onStatusChanged(VMRunner::Status status)
{
    if(status == VMRunner::Status::Paused)
    {
        const AxCore* core = m_impl->runner->core();
        assert(core && "Impossible path");

        const auto address = AxCore::pc_to_wram(core->registers().pc);
        // addresses are implicitely sorted
        auto it = std::lower_bound(m_impl->lineToAddress.begin(), m_impl->lineToAddress.end(), address);
        if(it != m_impl->lineToAddress.end() && *it == address)
        {
            const auto* wram = static_cast<const uint32_t*>(core->memory().map(*core, address));

            QString comment{};
            const auto [first, second] = AxOpcode::analyze(wram[0], 0);
            for(auto&& operand : first.operands())
            {
                // clang-format off
                const auto visitors = ax_overloads{
                    [core, &comment](AxOpcodeArg::Reg reg)
                    {
                        comment.append(QString{"%1 = %2 | "}
                            .arg(format_as(reg))
                            .arg(core->registers().gpi[reg.id]));
                    },
                    [core, &comment](AxOpcodeArg::FReg reg)
                    {
                        comment.append(QString{"%1 = %2 | "}
                            .arg(format_as(reg))
                            .arg(core->registers().gpf[reg.id]));
                    },
                    [core, &comment](AxOpcodeArg::MDUReg reg)
                    {
                        comment.append(QString{"%1 = %2 | "}
                            .arg(format_as(reg))
                            .arg(core->registers().mdu[reg.id]));
                    },
                    [](auto&&) { } // ignore other alternative
                };
                // clang-format on
                std::visit(visitors, operand.value);
            }

            if(!comment.isEmpty())
            {
                comment.erase(comment.end() - 2, comment.end());
            }
            else
            {
                comment = "no information";
            }

            const auto lineNumber = std::distance(m_impl->lineToAddress.begin(), it);
            m_impl->highligher->setDebugComment(lineNumber, comment);
            const auto block = m_impl->textEdit->document()->findBlockByLineNumber(lineNumber);
            m_impl->textEdit->setTextCursor(QTextCursor{block});
        }
    }
    else
    {
        m_impl->highligher->clearDebugComment();
    }
}

#include "disassemblyview.moc"
