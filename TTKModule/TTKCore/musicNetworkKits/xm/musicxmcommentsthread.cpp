#include "musicxmcommentsthread.h"
#include "musicdownloadqueryxmthread.h"
#include "musicsemaphoreloop.h"

#///QJson import
#include "qjson/parser.h"

MusicXMSongCommentsThread::MusicXMSongCommentsThread(QObject *parent)
    : MusicDownLoadCommentsThread(parent)
{
    m_pageSize = 20;
}

QString MusicXMSongCommentsThread::getClassName()
{
    return staticMetaObject.className();
}

void MusicXMSongCommentsThread::startToSearch(const QString &name)
{
    M_LOGGER_INFO(QString("%1 startToSearch %2").arg(getClassName()).arg(name));
    MusicSemaphoreLoop loop;
    MusicDownLoadQueryXMThread *query = new MusicDownLoadQueryXMThread(this);
    query->setQueryAllRecords(false);
    query->setQuerySimplify(true);
    query->startToSearch(MusicDownLoadQueryThreadAbstract::MusicQuery, name);
    connect(query, SIGNAL(downLoadDataChanged(QString)), &loop, SLOT(quit()));
    loop.exec();

    m_rawData["songID"] = 0;
    if(!query->getMusicSongInfos().isEmpty())
    {
        m_rawData["songID"] = query->getMusicSongInfos().first().m_songId.toInt();
        startToPage(0);
    }
}

void MusicXMSongCommentsThread::startToPage(int offset)
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
                      MusicUtils::Algorithm::mdII(XM_SG_COMMIT_DATA_URL, false).arg(m_rawData["songID"].toInt())
                      .arg(offset + 1).arg(m_pageSize),
                      MusicUtils::Algorithm::mdII(XM_COMMIT_URL, false));
    if(!m_manager || m_stateCode != MusicNetworkAbstract::Init) return;
#ifndef QT_NO_SSL
    QSslConfiguration sslConfig = request.sslConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    request.setSslConfiguration(sslConfig);
#endif
    m_reply = m_manager->get( request );
    connect(m_reply, SIGNAL(finished()), SLOT(downLoadFinished()) );
    connect(m_reply, SIGNAL(error(QNetworkReply::NetworkError)),
                     SLOT(replyError(QNetworkReply::NetworkError)) );
}

void MusicXMSongCommentsThread::downLoadFinished()
{
    if(m_reply == nullptr)
    {
        deleteAll();
        return;
    }

    M_LOGGER_INFO(QString("%1 downLoadFinished").arg(getClassName()));
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

                QVariantMap paging = value["pagingVO"].toMap();
                m_pageTotal = paging["count"].toLongLong();

                QVariantList comments = value["commentVOList"].toList();
                foreach(const QVariant &comm, comments)
                {
                    if(comm.isNull())
                    {
                        continue;
                    }

                    if(m_interrupt) return;

                    MusicSongCommentItem comment;
                    value = comm.toMap();
                    comment.m_nickName = value["nickName"].toString();
                    comment.m_avatarUrl = value["avatar"].toString();

                    if(comment.m_avatarUrl.contains("https://"))
                    {
                        comment.m_avatarUrl.replace("https://", "http://");
                    }

                    comment.m_likedCount = QString::number(value["likes"].toLongLong());
                    comment.m_time = QString::number(value["gmtCreate"].toLongLong());
                    comment.m_content = value["message"].toString();

                    emit createSearchedItems(comment);
                }
            }
        }
    }

    emit downLoadDataChanged(QString());
    deleteAll();
    M_LOGGER_INFO(QString("%1 downLoadFinished deleteAll").arg(getClassName()));
}



MusicXMPlaylistCommentsThread::MusicXMPlaylistCommentsThread(QObject *parent)
    : MusicDownLoadCommentsThread(parent)
{
    m_pageSize = 20;
}

QString MusicXMPlaylistCommentsThread::getClassName()
{
    return staticMetaObject.className();
}

void MusicXMPlaylistCommentsThread::startToSearch(const QString &name)
{
    M_LOGGER_INFO(QString("%1 startToSearch %2").arg(getClassName()).arg(name));

    m_rawData["songID"] = name;
    startToPage(0);
}

void MusicXMPlaylistCommentsThread::startToPage(int offset)
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
                      MusicUtils::Algorithm::mdII(XM_PL_COMMIT_DATA_URL, false).arg(m_rawData["songID"].toInt())
                      .arg(offset + 1).arg(m_pageSize),
                      MusicUtils::Algorithm::mdII(XM_COMMIT_URL, false));
    if(!m_manager || m_stateCode != MusicNetworkAbstract::Init) return;
#ifndef QT_NO_SSL
    QSslConfiguration sslConfig = request.sslConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    request.setSslConfiguration(sslConfig);
#endif
    m_reply = m_manager->get( request );
    connect(m_reply, SIGNAL(finished()), SLOT(downLoadFinished()) );
    connect(m_reply, SIGNAL(error(QNetworkReply::NetworkError)),
                     SLOT(replyError(QNetworkReply::NetworkError)) );
}

void MusicXMPlaylistCommentsThread::downLoadFinished()
{
    if(m_reply == nullptr)
    {
        deleteAll();
        return;
    }

    M_LOGGER_INFO(QString("%1 downLoadFinished").arg(getClassName()));
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

                QVariantMap paging = value["pagingVO"].toMap();
                m_pageTotal = paging["count"].toLongLong();

                QVariantList comments = value["commentVOList"].toList();
                foreach(const QVariant &comm, comments)
                {
                    if(comm.isNull())
                    {
                        continue;
                    }

                    if(m_interrupt) return;

                    MusicSongCommentItem comment;
                    value = comm.toMap();
                    comment.m_nickName = value["nickName"].toString();
                    comment.m_avatarUrl = value["avatar"].toString();

                    if(comment.m_avatarUrl.contains("https://"))
                    {
                        comment.m_avatarUrl.replace("https://", "http://");
                    }

                    comment.m_likedCount = QString::number(value["likes"].toLongLong());
                    comment.m_time = QString::number(value["gmtCreate"].toLongLong());
                    comment.m_content = value["message"].toString();

                    emit createSearchedItems(comment);
                }
            }
        }
    }

    emit downLoadDataChanged(QString());
    deleteAll();
    M_LOGGER_INFO(QString("%1 downLoadFinished deleteAll").arg(getClassName()));
}
