#ifndef PINYINMATCH_H
#define PINYINMATCH_H

#include <QObject>
#include <QStringList>
#include <QStringView>

struct _pinyin_context_t;
struct _pinyin_instance_t;

class PinyinMatch : public QObject
{
    Q_OBJECT

public:
    PinyinMatch();
    ~PinyinMatch() override;

    QStringList GetPinyins(QChar hanzi) const;
    bool MatchStr(const QStringView &hanziStr, const QStringView &pinyinStr);

private:
    _pinyin_context_t *pinyinContext = nullptr;
    _pinyin_instance_t *pinyinInstance = nullptr;
};

#endif // PINYINMATCH_H
