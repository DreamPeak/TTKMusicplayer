#include "musicdownloadqueryxmplaylistthread.h"
#include "musicsemaphoreloop.h"
#include "musictime.h"
#///QJson import
#include "qjson/parser.h"

MusicDownLoadQueryXMPlaylistThread::MusicDownLoadQueryXMPlaylistThread(QObject *parent)
    : MusicDownLoadQueryPlaylistThread(parent)
{
    m_pageSize = 30;
    m_queryServer = "XiaMi";
}

QString MusicDownLoadQueryXMPlaylistThread::getClassName()
{
    return staticMetaObject.className();
}

void MusicDownLoadQueryXMPlaylistThread::startToSearch(QueryType type, const QString &playlist)
{
    if(type == MusicQuery)
    {
        startToSearch(playlist);
    }
    else
    {
        m_searchText = playlist;
        startToPage(0);
    }
}

void MusicDownLoadQueryXMPlaylistThread::startToPage(int offset)
{
    if(!m_manager)
    {
        return;
    }

    M_LOGGER_INFO(QString("%1 startToSearch %2").arg(getClassName()).arg(offset));
    deleteAll();
    m_pageTotal = 0;
    m_interrupt = true;

    QNetworkRequest request;
    if(!m_manager || m_stateCode != MusicNetworkAbstract::Init) return;
    makeTokenQueryUrl(&request,
                      MusicUtils::Algorithm::mdII(XM_PLAYLIST_DATA_URL, false).arg(m_searchText).arg(offset + 1).arg(m_pageSize),
                      MusicUtils::Algorithm::mdII(XM_PLAYLIST_URL, false));
    if(!m_manager || m_stateCode != MusicNetworkAbstract::Init) return;
#ifndef QT_NO_SSL
    QSslConfiguration sslConfig = request.sslConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    request.setSslConfiguration(sslConfig);
#endif
    m_reply = m_manager->get( request );
    connect(m_reply, SIGNAL(finished()), SLOT(downLoadFinished()));
    connect(m_reply, SIGNAL(error(QNetworkReply::NetworkError)),
                     SLOT(replyError(QNetworkReply::NetworkError)));
}

void MusicDownLoadQueryXMPlaylistThread::startToSearch(const QString &playlist)
{
    if(!m_manager)
    {
        return;
    }

    M_LOGGER_INFO(QString("%1 startToSearch %2").arg(getClassName()).arg(playlist));
    m_interrupt = true;

    QNetworkRequest request;
    if(!m_manager || m_stateCode != MusicNetworkAbstract::Init) return;
    makeTokenQueryUrl(&request,
                      MusicUtils::Algorithm::mdII(XM_PLAYLIST_A_DATA_URL, false).arg(playlist).arg(1).arg(m_pageSize),
                      MusicUtils::Algorithm::mdII(XM_PLAYLIST_A_URL, false));
    if(!m_manager || m_stateCode != MusicNetworkAbstract::Init) return;
#ifndef QT_NO_SSL
    QSslConfiguration sslConfig = request.sslConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    request.setSslConfiguration(sslConfig);
#endif
    QNetworkReply *reply = m_manager->get(request);
    connect(reply, SIGNAL(finished()), SLOT(getDetailsFinished()));
    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), SLOT(replyError(QNetworkReply::NetworkError)));
}

void MusicDownLoadQueryXMPlaylistThread::getPlaylistInfo(MusicPlaylistItem &item)
{
    if(!m_manager)
    {
        return;
    }

    M_LOGGER_INFO(QString("%1 getPlaylistInfo %2").arg(getClassName()).arg(item.m_id));
    QNetworkRequest request;
    if(!m_manager || m_stateCode != MusicNetworkAbstract::Init) return;
    makeTokenQueryUrl(&request,
                      MusicUtils::Algorithm::mdII(XM_PLAYLIST_A_DATA_URL, false).arg(item.m_id).arg(1).arg(m_pageSize),
                      MusicUtils::Algorithm::mdII(XM_PLAYLIST_A_URL, false));
    if(!m_manager || m_stateCode != MusicNetworkAbstract::Init) return;
#ifndef QT_NO_SSL
    QSslConfiguration sslConfig = request.sslConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    request.setSslConfiguration(sslConfig);
#endif
    MusicSemaphoreLoop loop;
    QNetworkReply *reply = m_manager->get(request);
    QObject::connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    QObject::connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), &loop, SLOT(quit()));
    loop.exec();

    if(!reply || reply->error() != QNetworkReply::NoError)
    {
        return;
    }

    QByteArray bytes = reply->readAll();

    QJson::Parser parser;
    bool ok;
    QVariant data = parser.parse(bytes, &ok);
    if(ok)
    {
        QVariantMap value = data.toMap();
        if(value.contains("data"))
        {
            value = value["data"].toMap();
            value = value["data"].toMap();
            value = value["collectDetail"].toMap();

            item.m_coverUrl = value["collectLogo"].toString();
            item.m_name = value["collectName"].toString();
            item.m_playCount = QString::number(value["playCount"].toULongLong());
            item.m_description = value["description"].toString();
            item.m_updateTime = QDateTime::fromMSecsSinceEpoch(value["gmtModify"].toULongLong()).toString("yyyy-MM-dd");
            item.m_nickname = value["userName"].toString();

            QVariantList tags = value["tags"].toList();
            item.m_tags.clear();
            foreach(const QVariant &var, tags)
            {
                if(var.isNull())
                {
                    continue;
                }
                item.m_tags.append(var.toString() + "|");
            }
        }
    }
}

void MusicDownLoadQueryXMPlaylistThread::downLoadFinished()
{
    if(!m_reply)
    {
        deleteAll();
        return;
    }

    M_LOGGER_INFO(QString("%1 downLoadFinished").arg(getClassName()));
    emit clearAllItems();      ///Clear origin items
    m_musicSongInfos.clear();  ///Empty the last search to songsInfo
    m_interrupt = false;

    if(m_reply->error() == QNetworkReply::NoError)
    {
        QByteArray bytes = m_reply->readAll(); ///Get all the data obtained by request

        QJson::Parser parser;
        bool ok;
        QVariant data = parser.parse(bytes, &ok);
        if(ok)
        {
            QVariantMap value = data.toMap();
            if(value.contains("data"))
            {
                value = value["data"].toMap();
                value = value["data"].toMap();
                m_pageTotal = value["total"].toLongLong();

                QVariantList datas = value["collects"].toList();
                foreach(const QVariant &var, datas)
                {
                    if(var.isNull())
                    {
                        continue;
                    }

                    if(m_interrupt) return;

                    value = var.toMap();
                    MusicPlaylistItem item;
                    item.m_coverUrl = value["collectLogo"].toString();
                    item.m_id = QString::number(value["listId"].toULongLong());
                    item.m_name = value["collectName"].toString();
                    item.m_playCount = QString::number(value["playCount"].toULongLong());
                    item.m_description = value["description"].toString();
                    item.m_updateTime = QDateTime::fromMSecsSinceEpoch(value["gmtModify"].toULongLong()).toString("yyyy-MM-dd");
                    item.m_nickname = value["userName"].toString();

                    QVariantList tags = value["tags"].toList();
                    item.m_tags.clear();
                    foreach(const QVariant &var, tags)
                    {
                        if(var.isNull())
                        {
                            continue;
                        }
                        item.m_tags.append(var.toString() + "|");
                    }

                    emit createPlaylistItems(item);
                }
            }
        }
    }

//    emit downLoadDataChanged(QString());
    deleteAll();
    M_LOGGER_INFO(QString("%1 downLoadFinished deleteAll").arg(getClassName()));
}

void MusicDownLoadQueryXMPlaylistThread::getDetailsFinished()
{
    M_LOGGER_INFO(QString("%1 getDetailsFinished").arg(getClassName()));
    QNetworkReply *reply = MObject_cast(QNetworkReply*, QObject::sender());

    emit clearAllItems();      ///Clear origin items
    m_musicSongInfos.clear();  ///Empty the last search to songsInfo
    m_interrupt = false;

    if(reply && m_manager && reply->error() == QNetworkReply::NoError)
    {
        QByteArray bytes = reply->readAll();

        QJson::Parser parser;
        bool ok;
        QVariant data = parser.parse(bytes, &ok);
        if(ok)
        {
            QVariantMap value = data.toMap();
            if(value.contains("data"))
            {
                value = value["data"].toMap();
                value = value["data"].toMap();
                value = value["collectDetail"].toMap();
                QVariantList datas = value["songs"].toList();
                foreach(const QVariant &var, datas)
                {
                    if(var.isNull())
                    {
                        continue;
                    }

                    value = var.toMap();
                    MusicObject::MusicSongInformation musicInfo;
                    musicInfo.m_singerName = value["singers"].toString();
                    musicInfo.m_songName = value["songName"].toString();
                    musicInfo.m_timeLength = MusicTime::msecTime2LabelJustified(value["length"].toInt());

                    musicInfo.m_songId = value["songId"].toString();
                    musicInfo.m_artistId = value["artistId"].toString();
                    musicInfo.m_albumId = value["albumId"].toString();
                    musicInfo.m_albumName = value["albumName"].toString();

                    musicInfo.m_smallPicUrl = value["albumLogo"].toString();

                    if(m_interrupt || !m_manager || m_stateCode != MusicNetworkAbstract::Init) return;
                    readFromMusicSongAttribute(&musicInfo, value["listenFiles"], m_searchQuality, m_queryAllRecords);
                    if(m_interrupt || !m_manager || m_stateCode != MusicNetworkAbstract::Init) return;

                    if(musicInfo.m_songAttrs.isEmpty())
                    {
                        continue;
                    }

                    MusicSearchedItem item;
                    item.m_songName = musicInfo.m_songName;
                    item.m_singerName = musicInfo.m_singerName;
                    item.m_albumName = musicInfo.m_albumName;
                    item.m_time = musicInfo.m_timeLength;
                    item.m_type = mapQueryServerString();
                    emit createSearchedItems(item);
                    m_musicSongInfos << musicInfo;
                }
            }
        }
    }

    emit downLoadDataChanged(QString());
    M_LOGGER_INFO(QString("%1 getDetailsFinished deleteAll").arg(getClassName()));
}
