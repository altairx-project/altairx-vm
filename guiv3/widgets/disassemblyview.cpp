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
        const auto commentPattern = QStringLiteral(";[^\n]*");
        QTextCharFormat commentFormat{};
        commentFormat.setForeground(Qt::darkGreen);
        m_rules.emplace_back(QRegularExpression(commentPattern), commentFormat);

        const auto stringPattern = QStringLiteral("\"(?:[^\\\\\"]|\\\\.)*\"");
        QTextCharFormat stringFormat{};
        stringFormat.setForeground(Qt::darkRed);
        m_rules.emplace_back(QRegularExpression(stringPattern), stringFormat);

        const auto labelPattern = QStringLiteral("\\b\\w*:");
        QTextCharFormat labelFormat{};
        labelFormat.setForeground(Qt::blue);
        labelFormat.setFontWeight(QFont::Bold);
        m_rules.emplace_back(QRegularExpression(labelPattern), labelFormat);

        const auto constantPattern = QStringLiteral("\\.\\w+");
        QTextCharFormat constantFormat{};
        constantFormat.setForeground(Qt::darkGreen);
        m_rules.emplace_back(QRegularExpression(constantPattern), constantFormat);

        const auto controlKeywordPattern = QStringLiteral("\\b(bne|beq|bl|ble|bg|bge|bls|bles|bgs|bges|bra|loop|jump|call|jumpbr|callbr|ret|reti|syscall|int)\\b");
        QTextCharFormat controlKeywordFormat{};
        controlKeywordFormat.setForeground(Qt::darkMagenta);
        controlKeywordFormat.setFontWeight(QFont::Bold);
        m_rules.emplace_back(QRegularExpression(controlKeywordPattern), controlKeywordFormat);

        const auto fixedKeywordPattern = QStringLiteral("\\b(nop|moveix|ext|ins|movei|moven|umove|move)\\b");
        QTextCharFormat fixedKeywordFormat{};
        fixedKeywordFormat.setForeground(Qt::darkCyan);
        fixedKeywordFormat.setFontWeight(QFont::Bold);
        m_rules.emplace_back(QRegularExpression(fixedKeywordPattern), fixedKeywordFormat);

        const auto sizedKeywordPattern = QStringLiteral("\\b(add|sub|xor|or|and|lsl|asr|lsr|se|slts|sltu|sand|hadd|hsub|hmul|hto|cmp|test|cmpfr|testfr|cmove|sext|rtl|rtr|adc|sbc|ld|st|ldv|stv|lds|sts|ldvs|stvs|div|divu|mul|mulu)(\\.[bwdq])?\\b");
        QTextCharFormat sizedKeywordFormat{};
        sizedKeywordFormat.setForeground(Qt::darkCyan);
        sizedKeywordFormat.setFontWeight(QFont::Bold);
        m_rules.emplace_back(QRegularExpression(sizedKeywordPattern), sizedKeywordFormat);

        const auto variablePattern = QStringLiteral("\\b(r|a|t|n|s|S|R|A|T|N|sp|SP)\\d+\\b");
        QTextCharFormat variableFormat{};
        variableFormat.setForeground(Qt::darkYellow);
        m_rules.emplace_back(QRegularExpression(variablePattern), variableFormat);

        const auto languageVariablePattern = QStringLiteral("\\b(LR|lr|BR|br|LC|lc|FR|fr|PC|pc|IR|ir|CC|cc|IC|ic|PL|pl|PH|ph|PQ|pq|PR|pr)\\b");
        QTextCharFormat languageVariableFormat{};
        languageVariableFormat.setForeground(Qt::darkRed);
        m_rules.emplace_back(QRegularExpression(languageVariablePattern), languageVariableFormat);

        const auto numericsPattern = QStringLiteral("\\b(0x|0X|0b|0B|0o|0O|\\d)[\\dA-Fa-f]*(\\.\\d*(f|)|)\\b");
        QTextCharFormat numericsFormat{};
        numericsFormat.setForeground(Qt::darkRed);
        m_rules.emplace_back(QRegularExpression(numericsPattern), numericsFormat);

        const auto operatorPattern = QStringLiteral(",");
        QTextCharFormat operatorFormat{};
        operatorFormat.setForeground(Qt::darkGray);
        m_rules.emplace_back(QRegularExpression(operatorPattern), operatorFormat);
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

    const uint32_t* wram = reinterpret_cast<const uint32_t*>(core->memory().map(*core, AxMemory::WRAM_BEGIN));

    uint64_t lastAddress{};
    // Disassemble program
    for(const AxCore::Symbol& symbol : core->symbols())
    {
        if(symbol.address == lastAddress)
        {
            continue; // skip aliases
        }

        m_impl->textEdit->appendPlainText(QString{"%1:"}.arg(symbol.name));

        uint64_t currentOffset{};
        while(currentOffset < symbol.size)
        {
            const auto currentAddr = AxMemory::WRAM_BEGIN + symbol.address + currentOffset;
            const auto currentIndex = (symbol.address + currentOffset) / 4ull;

            if(currentOffset == 0)
            {
                m_impl->lineToAddress.emplace_back(currentAddr); // this is for the symbol
            }

            auto [first, second] = AxOpcode::to_string(wram[currentIndex], wram[currentIndex + 1u]);
            if(first.empty())
            {
                break; // failed to decode, continue to next symbol
            }

            m_impl->lineToAddress.emplace_back(currentAddr); // this is for the asm line
            m_impl->textEdit->appendPlainText(QString{"0x%1\t%2"}.arg(currentAddr, 16, 16, u'0').arg(first));
            currentOffset += 4;

            if(!second.empty())
            {
                m_impl->lineToAddress.emplace_back(currentAddr + 4ull); // this is for the asm line
                m_impl->textEdit->appendPlainText(QString{"0x%1\t%2"}.arg(currentAddr + 4ull, 16, 16, u'0').arg(second));
                currentOffset += 4;
            }
        }

        lastAddress = symbol.address;
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
                const auto visitors = ax_overloads{
                    [core, &comment](AxOpcodeArg::Reg reg)
                {
                    comment.append(QString{"%1 = %2; "}.arg(format_as(reg)).arg(core->registers().gpi[reg.id]));
                },
                    [](auto&&)
                {
                }};
                std::visit(visitors, operand.value);
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
