#include "pinyinmatch.h"

#include <pinyin.h>

#include <QDir>
#include <QFile>
#include <QStandardPaths>

PinyinMatch::PinyinMatch()
{
    const QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
    if (cacheDir.isEmpty())
        return;

    const QString userDataDir = QDir(cacheDir).filePath(QStringLiteral("pinyinmatch/libpinyin"));
    if (!QDir().mkpath(userDataDir))
        return;

    const QByteArray systemDir = QFile::encodeName(QStringLiteral(LIBPINYIN_DATA_DIR));
    const QByteArray userDir = QFile::encodeName(userDataDir);
    pinyinContext = pinyin_init(systemDir.constData(), userDir.constData());
    if (!pinyinContext)
        return;

    pinyinInstance = pinyin_alloc_instance(pinyinContext);
    if (!pinyinInstance) {
        pinyin_fini(pinyinContext);
        pinyinContext = nullptr;
    }
}

PinyinMatch::~PinyinMatch()
{
    if (pinyinInstance)
        pinyin_free_instance(pinyinInstance);
    if (pinyinContext)
        pinyin_fini(pinyinContext);
}

QStringList PinyinMatch::GetPinyins(QChar hanzi) const
{
    QStringList pronunciations;
    if (!pinyinInstance || hanzi.isSurrogate())
        return pronunciations;

    const QByteArray phrase = QString(hanzi).toUtf8();
    GArray *tokens = g_array_new(FALSE, FALSE, sizeof(phrase_token_t));
    if (!pinyin_lookup_tokens(pinyinInstance, phrase.constData(), tokens)) {
        g_array_free(tokens, TRUE);
        return pronunciations;
    }

    for (guint tokenIndex = 0; tokenIndex < tokens->len; ++tokenIndex) {
        const phrase_token_t token = g_array_index(tokens, phrase_token_t, tokenIndex);
        guint pronunciationCount = 0;
        if (!pinyin_token_get_n_pronunciation(pinyinInstance, token, &pronunciationCount)) {
            continue;
        }

        for (guint pronunciationIndex = 0; pronunciationIndex < pronunciationCount; ++pronunciationIndex) {
            // ChewingKey is an opaque 16-bit value in libpinyin's public ABI.
            GArray *keys = g_array_new(FALSE, FALSE, sizeof(guint16));
            const bool found = pinyin_token_get_nth_pronunciation(pinyinInstance, token, pronunciationIndex, keys);

            if (found && keys->len == 1) {
                gchar *rawPinyin = nullptr;
                auto *key = reinterpret_cast<ChewingKey *>(keys->data);
                if (pinyin_get_pinyin_string(pinyinInstance, key, &rawPinyin)) {
                    QString pronunciation = QString::fromUtf8(rawPinyin);
                    while (!pronunciation.isEmpty() && pronunciation.back().isDigit()) {
                        pronunciation.chop(1);
                    }

                    if (!pronunciation.isEmpty() && !pronunciations.contains(pronunciation)) {
                        pronunciations.append(pronunciation);
                    }
                }
                g_free(rawPinyin);
            }

            g_array_free(keys, TRUE);
        }
    }

    g_array_free(tokens, TRUE);
    return pronunciations;
}

bool PinyinMatch::MatchStr(const QStringView &hanziStr, const QStringView &pinyinStr)
{
    if (pinyinStr.empty())
        return true;
    if (hanziStr.empty())
        return false;

    const auto hanzi = hanziStr[0];
    if (hanzi == pinyinStr[0]) {
        if (MatchStr(hanziStr.sliced(1), pinyinStr.sliced(1)))
            return true;
    }

    const auto correspondPinyinList = GetPinyins(hanzi);
    for (const auto &correspondPinyin : correspondPinyinList) {
        if (pinyinStr[0] == correspondPinyin[0]) {
            if (MatchStr(hanziStr.sliced(1), pinyinStr.sliced(1)))
                return true;
        }

        if (pinyinStr.size() <= correspondPinyin.size()) {
            if (correspondPinyin.startsWith(pinyinStr))
                return true;
        } else if (pinyinStr.startsWith(correspondPinyin)) {
            if (MatchStr(hanziStr.sliced(1), pinyinStr.sliced(correspondPinyin.size())))
                return true;
        }
    }
    return false;
}
