/*
  The MIT License (MIT)

  Copyright (c) 2015-2016 Andrea Scarpino <me@andreascarpino.it>

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include "lyricsmanager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>

#include "chartlyricsapi.h"
#include "geniusapi.h"
#include "lyricsmaniaapi.h"
#include "mediaplayerscanner.h"
#include "provider.h"

LyricsManager::LyricsManager(QObject *parent) :
    QObject(parent)
  , api(0)
  , mpScanner(0)
{
    settings = new QSettings(QCoreApplication::applicationName(), QCoreApplication::applicationName(), this);

    setProvider(settings->value("Provider").toString());
    setMediaPlayerScanner(settings->value("MediaPlayerScanner", true).toBool());
}

LyricsManager::~LyricsManager()
{
    delete settings;
    delete api;
    delete mpScanner;
}

void LyricsManager::clearCache()
{
    const bool ret = QDir(getLyricsDir()).removeRecursively();
    qDebug() << "Cache cleared:" << ret;
}

QString LyricsManager::getProvider() const
{
    QString provider;

    const QString className = api->metaObject()->className();
    if (className.compare(QStringLiteral("ChartLyricsAPI")) == 0) {
        provider = "ChartLyrics";
    } else if (className.compare(QStringLiteral("GeniusAPI")) == 0) {
        provider = "Genius";
    } else {
        provider = "LyricsMania";
    }

    qDebug() << "Default provider is" << provider;
    return provider;
}

void LyricsManager::setProvider(const QString &provider)
{
    if (api) {
        delete api;
    }

    QString p;
    if (provider.compare(QStringLiteral("ChartLyrics")) == 0) {
        api = new ChartLyricsAPI;
        p = QStringLiteral("ChartLyrics");
    } else if (provider.compare(QStringLiteral("Genius")) == 0) {
        api = new GeniusAPI;
        p = QStringLiteral("Genius");
    } else {
        api = new LyricsManiaAPI;
        p = QStringLiteral("LyricsMania");
    }

    connect(api, &Provider::lyricFetched, this, &LyricsManager::searchResult);
    connect(api, &Provider::lyricFetched, this, &LyricsManager::storeLyric);

    qDebug() << "Setting default provider to" << p;
    settings->setValue("Provider", p);
}

QString LyricsManager::getLyricsDir() const
{
    const QDir lyricsDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));

    if (!lyricsDir.exists()) {
        if (!lyricsDir.mkpath(lyricsDir.absolutePath())) {
            qCritical() << "Cannot create dir!";
        }
    }

    return QString(lyricsDir.absolutePath() + QDir::separator());
}

void LyricsManager::storeLyric(Lyric *lyric, const bool &found)
{
    if (found) {
        qDebug() << "Caching lyric by" << lyric->artist() << lyric->song();
        QFile f(getLyricsDir() + lyric->artist() + "_" + lyric->song() + ".txt");
        f.open(QIODevice::WriteOnly);
        f.write(lyric->text().toLatin1());
        f.close();
    }
}

void LyricsManager::search(const QString &artist, const QString &song)
{
    QFile f(getLyricsDir() + artist + "_" + song + ".txt");
    if (f.exists()) {
        f.open(QIODevice::ReadOnly);

        Lyric* lyric = new Lyric();
        lyric->setArtist(artist);
        lyric->setSong(song);
        lyric->setText(f.readAll());
        f.close();

        Q_EMIT searchResult(lyric, true);
    } else {
        qDebug() << "Querying" << api->metaObject()->className();
        api->getLyric(artist, song);
    }
}

bool LyricsManager::getMediaPlayerScanner() const
{
    return settings->value("MediaPlayerScanner").toBool();
}

void LyricsManager::setMediaPlayerScanner(const bool enabled)
{
    if (enabled) {
        mpScanner = new MediaPlayerScanner();
        connect(mpScanner, &MediaPlayerScanner::mediaPlayerInfo, this, &LyricsManager::mediaPlayerInfo);
    } else if (mpScanner) {
        delete mpScanner;
        mpScanner = 0;
    }

    settings->setValue("MediaPlayerScanner", enabled);
}
