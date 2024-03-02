/*
 * Copyright (c) 2010 Justin Knotzke (jknotzke@shampoo.ca)
 * Copyright (c) 2017 Mark Liversedge (liversedge@gmail.com)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include "Secrets.h"

#include "OAuthDialog.h"
#include "Athlete.h"
#include "Context.h"
#include "Settings.h"
#include "Colors.h"
#include "TimeUtils.h"

#include "PolarFlow.h"

#include <QJsonParseError>

OAuthDialog::OAuthDialog(Context *context, OAuthSite site, CloudService *service, QString baseURL, QString clientsecret) :
    context(context), site(site), service(service), baseURL(baseURL), clientsecret(clientsecret)
{

    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("OAuth"));

    // we need to scale up, since we zoom the webview on hi-dpi screens
    setMinimumSize(640 *dpiXFactor, 640 *dpiYFactor);

    if (service) { // ultimately this will be the only way this works
        if (service->id() == "Strava") site = this->site = STRAVA;
        if (service->id() == "Dropbox") site = this->site = DROPBOX;
        if (service->id() == "Cycling Analytics") site = this->site = CYCLING_ANALYTICS;
        if (service->id() == "Nolio") site = this->site = NOLIO;
        if (service->id() == "Withings") site = this->site = WITHINGS;
        if (service->id() == "PolarFlow") site = this->site = POLAR;
        if (service->id() == "SportTracks.mobi") site = this->site = SPORTTRACKS;
        if (service->id() == "Xert") site = this->site = XERT;
        if (service->id() == "RideWithGPS") site = this->site = RIDEWITHGPS;
        if (service->id() == "Azum") site = this->site = AZUM;
    }

    // check if SSL is available - if not - message and end
    if (!QSslSocket::supportsSsl()) {
        QString text = QString(tr("SSL Security Libraries required for 'Authorise' are missing in this installation."));
        QMessageBox sslMissing(QMessageBox::Critical, tr("Authorization Error"), text);
        sslMissing.exec();
        noSSLlib = true;
        return;
    }

    // ignore responses to false, used by POLARFLOW when binding the user
    ignore = false;

    // SSL is available - so authorisation can take place
    noSSLlib = false;

    layout = new QVBoxLayout();
    layout->setSpacing(0);
    layout->setContentsMargins(2,0,2,2);
    setLayout(layout);


    view = new QWebEngineView();
    view->setZoomFactor(dpiXFactor);
    view->page()->profile()->cookieStore()->deleteAllCookies();

    view->setContentsMargins(0,0,0,0);
    view->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    view->setAcceptDrops(false);
    layout->addWidget(view);

    //
    // All services have some kind of initial authorisation URL where the user needs
    // to login and confirm they are willing to authorise the particular app and
    // provide a temporary token to get the real token with
    //
    QString urlstr = "";

    if (site == STRAVA) {

        urlstr = QString("https://www.strava.com/oauth/authorize?");
        urlstr.append("client_id=").append(GC_STRAVA_CLIENT_ID).append("&");
        urlstr.append("scope=activity:read_all,activity:write&");
        urlstr.append("redirect_uri=http://www.goldencheetah.org/&");
        urlstr.append("response_type=code&");
        urlstr.append("approval_prompt=force");

    } else if(site == NOLIO) {

        urlstr = QString("https://www.nolio.io/api/authorize/?");
        urlstr.append("client_id=").append(GC_NOLIO_CLIENT_ID).append("&");
        urlstr.append("redirect_uri=http://www.goldencheetah.org/&");
        urlstr.append("response_type=code");

    } else if (site == DROPBOX) {

        urlstr = QString("https://www.dropbox.com/oauth2/authorize?");
#ifdef GC_DROPBOX_CLIENT_ID
        urlstr.append("client_id=").append(GC_DROPBOX_CLIENT_ID).append("&");
#endif
        urlstr.append("redirect_uri=https://goldencheetah.github.io/blank.html&");
        urlstr.append("response_type=code&");
        urlstr.append("force_reapprove=true");

    } else if (site == CYCLING_ANALYTICS) {

        urlstr = QString("https://www.cyclinganalytics.com/api/auth?");
        urlstr.append("client_id=").append(GC_CYCLINGANALYTICS_CLIENT_ID).append("&");
        urlstr.append("scope=modify_rides&");
        urlstr.append("redirect_uri=http://www.goldencheetah.org/&");
        urlstr.append("response_type=code&");
        urlstr.append("approval_prompt=force");

    } else if (site == POLAR) {

        // OAUTH 2.0 - Polar flow for installed applications
        urlstr = QString("%1?").arg(GC_POLARFLOW_OAUTH_URL);
        // We only request access to the application data folder, not all files.
        urlstr.append("response_type=code&");
        urlstr.append("client_id=").append(GC_POLARFLOW_CLIENT_ID);

    } else if (site == SPORTTRACKS) {

        // We only request access to the application data folder, not all files.
        urlstr = QString("https://api.sporttracks.mobi/oauth2/authorize?");
        urlstr.append("redirect_uri=http://www.goldencheetah.org&");
        urlstr.append("state=xyzzy&");
        urlstr.append("response_type=code&");
        urlstr.append("client_id=").append(GC_SPORTTRACKS_CLIENT_ID);

    } else if (site == WITHINGS) {

        urlstr = QString("https://account.withings.com/oauth2_user/authorize2?");
        urlstr.append("redirect_uri=https://www.goldencheetah.org&");
        urlstr.append("scope=user.info,user.metrics&");
        urlstr.append("response_type=code&");
        urlstr.append("state=xyzzy&");
        urlstr.append("client_id=").append(GC_NOKIA_CLIENT_ID);

    } else if (site == XERT) {
        urlChanged(QUrl("http://www.goldencheetah.org/?code=0"));
    } else if (site == RIDEWITHGPS) {
        urlChanged(QUrl("http://www.goldencheetah.org/?code=0"));
    } else if (site == AZUM) {
        if (baseURL=="") baseURL=service->getSetting(GC_AZUM_URL, "https://training.azum.com").toString();
        urlstr = QString("%1/oauth/authorize/?").arg(baseURL);

        urlstr.append("redirect_uri=http://www.goldencheetah.org/&");
        urlstr.append("response_type=code&");
        urlstr.append("client_id=").append(GC_AZUM_CLIENT_ID);
    }

    //
    // STEP 1: LOGIN AND AUTHORISE THE APPLICATION
    //
    if (site == NOLIO || site == DROPBOX || site == STRAVA || site == CYCLING_ANALYTICS || site == POLAR || site == SPORTTRACKS || site == WITHINGS || site == AZUM) {
        url = QUrl(urlstr);
        view->setUrl(url);
        // connects
        connect(view, SIGNAL(urlChanged(const QUrl&)), this, SLOT(urlChanged(const QUrl&)));
    }
}

OAuthDialog::~OAuthDialog()
{
  if (view) delete view->page();
  delete view;  // view was constructed without a parent to delete it
}

// just ignore SSL handshake errors at all times
void
OAuthDialog::onSslErrors(QNetworkReply *reply, const QList<QSslError>&)
{
    reply->ignoreSslErrors();
}


//
// STEP 2: AUTHORISATION REDIRECT WITH TEMPORARY CODE
//
// When the URL changes, it will be the redirect with the temporary token after
// the initial authorisation. The URL will have some parameters to indicate this
// if they exist we should intercept the redirect to get the permanent token.
// If they don't get passed then we don't need to do anything.
//
void
OAuthDialog::urlChanged(const QUrl &url)
{
    QString authheader;

    // sites that use this scheme
    if (site == NOLIO || site == DROPBOX || site == STRAVA || site == CYCLING_ANALYTICS || site == POLAR || site == SPORTTRACKS || site == XERT || site == RIDEWITHGPS || site == WITHINGS || site == AZUM) {

        if (url.toString().startsWith("http://www.goldencheetah.org/?state=&code=") ||
                url.toString().contains("blank.html?code=") ||
                url.toString().startsWith("https://www.goldencheetah.org/?code=") ||
                url.toString().startsWith("http://www.goldencheetah.org/?code=")) {

            QUrlQuery parse(url);
            QString code=parse.queryItemValue("code");

            QByteArray data;
            QUrlQuery params;
            QString urlstr = "";

            // now get the final token to store
            if (site == DROPBOX) {

                urlstr = QString("https://api.dropboxapi.com/oauth2/token?");
                urlstr.append("redirect_uri=https://goldencheetah.github.io/blank.html&");
                params.addQueryItem("grant_type", "authorization_code");
#ifdef GC_DROPBOX_CLIENT_ID
                params.addQueryItem("client_id", GC_DROPBOX_CLIENT_ID);
#endif
#ifdef GC_DROPBOX_CLIENT_SECRET
                params.addQueryItem("client_secret", GC_DROPBOX_CLIENT_SECRET);
#endif

            } else if (site == POLAR) {

                urlstr = QString("%1?").arg(GC_POLARFLOW_TOKEN_URL);
                urlstr.append("redirect_uri=http://www.goldencheetah.org");
                params.addQueryItem("grant_type", "authorization_code");
#if (defined GC_POLARFLOW_CLIENT_ID) && (defined GC_POLARFLOW_CLIENT_SECRET)
                authheader = QString("%1:%2").arg(GC_POLARFLOW_CLIENT_ID).arg(GC_POLARFLOW_CLIENT_SECRET);
#endif

            } else if (site == SPORTTRACKS) {

                urlstr = QString("https://api.sporttracks.mobi/oauth2/token?");
                params.addQueryItem("client_id", GC_SPORTTRACKS_CLIENT_ID);
                params.addQueryItem("client_secret", GC_SPORTTRACKS_CLIENT_SECRET);
                params.addQueryItem("redirect_uri","http://www.goldencheetah.org");
                params.addQueryItem("grant_type", "authorization_code");

            } else if (site == STRAVA) {

                urlstr = QString("https://www.strava.com/oauth/token?");
                params.addQueryItem("client_id", GC_STRAVA_CLIENT_ID);
#ifdef GC_STRAVA_CLIENT_SECRET
                params.addQueryItem("client_secret", GC_STRAVA_CLIENT_SECRET);
#endif

            } else if (site == NOLIO) {

                urlstr = QString("https://www.nolio.io/api/token/?");
                params.addQueryItem("grant_type", "authorization_code");
                params.addQueryItem("redirect_uri", "http://www.goldencheetah.org/");
#if (defined GC_NOLIO_CLIENT_ID) && (defined GC_NOLIO_SECRET)
                authheader = QString("%1:%2").arg(GC_NOLIO_CLIENT_ID).arg(GC_NOLIO_SECRET);
#endif

            }  else if (site == CYCLING_ANALYTICS) {

                urlstr = QString("https://www.cyclinganalytics.com/api/token?");
                params.addQueryItem("client_id", GC_CYCLINGANALYTICS_CLIENT_ID);
#ifdef GC_CYCLINGANALYTICS_CLIENT_SECRET
                params.addQueryItem("client_secret", GC_CYCLINGANALYTICS_CLIENT_SECRET);
#endif
                params.addQueryItem("grant_type", "authorization_code");

            } else if (site == XERT) {

                urlstr = QString("https://www.xertonline.com/oauth/token");
                params.addQueryItem("username", service->getSetting(GC_XERTUSER, "").toString());
                params.addQueryItem("password", service->getSetting(GC_XERTPASS, "").toString());
                params.addQueryItem("grant_type", "password");

                authheader = QString("%1:%2").arg(GC_XERT_CLIENT_ID).arg(GC_XERT_CLIENT_SECRET);
            } else if (site == RIDEWITHGPS) {
                urlstr = QString("https://ridewithgps.com/users/current.json");
                params.addQueryItem("apikey", GC_RWGPS_API_KEY);
                params.addQueryItem("version", "2");
                params.addQueryItem("email", service->getSetting(GC_RWGPSUSER, "").toString());
                params.addQueryItem("password", service->getSetting(GC_RWGPSPASS, "").toString());

            } else if (site == WITHINGS) {

                urlstr = QString("https://wbsapi.withings.net/v2/oauth2");
                params.addQueryItem("client_id", GC_NOKIA_CLIENT_ID);
                params.addQueryItem("client_secret", GC_NOKIA_CLIENT_SECRET);
                params.addQueryItem("redirect_uri","https://www.goldencheetah.org");
                params.addQueryItem("action", "requesttoken");
                params.addQueryItem("grant_type", "authorization_code");

            } else if (site == AZUM) {

                if (baseURL=="") baseURL=service->getSetting(GC_AZUM_URL, "https://training.azum.com").toString();
                urlstr = QString("%1/oauth/token/?").arg(baseURL);
                params.addQueryItem("client_id", GC_AZUM_CLIENT_ID);
                if (service->getSetting(GC_AZUM_USERKEY, "").toString() != "") {
                    params.addQueryItem("client_secret", service->getSetting(GC_AZUM_USERKEY, "").toString());
                } else {
                    params.addQueryItem("client_secret", GC_AZUM_CLIENT_SECRET);
                }
                params.addQueryItem("redirect_uri","http://www.goldencheetah.org/");
                params.addQueryItem("grant_type", "authorization_code");
            }

            // all services will need us to send the temporary code received
            params.addQueryItem("code", code);

            data.append(params.query(QUrl::FullyEncoded).toUtf8());

            // trade-in the temporary access code retrieved by the Call-Back URL for the finale token
            QUrl url = QUrl(urlstr);

            // now get the final token - but ignore errors
            manager = new QNetworkAccessManager(this);
            connect(manager, SIGNAL(sslErrors(QNetworkReply*, const QList<QSslError> & )), this, SLOT(onSslErrors(QNetworkReply*, const QList<QSslError> & )));
            connect(manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkRequestFinished(QNetworkReply*)));

            if (site == RIDEWITHGPS) {
                url.setQuery(data);
                QNetworkRequest request = QNetworkRequest(url);
                manager->get(request);
            } else {
                QNetworkRequest request = QNetworkRequest(url);
                request.setHeader(QNetworkRequest::ContentTypeHeader,"application/x-www-form-urlencoded");

                // client id and secret are encoded and sent in the header for NOLIO, POLAR and XERT
                if (site == NOLIO || site == POLAR || site == XERT)  request.setRawHeader("Authorization", "Basic " +  authheader.toLatin1().toBase64());
                manager->post(request, data);
            }

        }
    }
}

//
// STEP 3: REFRESH AND ACCESS TOKEN RECEIVED
//
// this is when we get the refresh or access tokens after a redirect has been loaded
// so pretty much at the end of the process. Each service can have slightly special
// needs and certainly needs to set the right setting anyway.
//
void
OAuthDialog::networkRequestFinished(QNetworkReply *reply)
{

    // we've been told to ignore responses (used by POLAR, maybe others in future)
    if (ignore) return;
    // we can handle SSL handshake errors, if we got here then some kind of protocol was agreed
    if (reply->error() == QNetworkReply::NoError || reply->error() == QNetworkReply::SslHandshakeFailedError) {

        QByteArray payload = reply->readAll(); // JSON
        QString refresh_token;
        QString access_token;
        QString auth_token;
        double polar_userid=0;

        // parse the response and extract the tokens, pretty much the same for all services
        // although polar choose to also pass a user id, which is needed for future calls
        QJsonParseError parseError;
        QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
        if (parseError.error == QJsonParseError::NoError) {
            refresh_token = document.object()["refresh_token"].toString();
            access_token = document.object()["access_token"].toString();
            if (site == POLAR)  polar_userid = document.object()["x_user_id"].toDouble();
            if (site == RIDEWITHGPS) access_token = document.object()["user"].toObject()["auth_token"].toString();
            if (site == WITHINGS) {
                refresh_token = document.object()["body"].toObject()["refresh_token"].toString();
                access_token = document.object()["body"].toObject()["access_token"].toString();
            }
        }

        // now set the tokens etc
        if (site == DROPBOX) {

            service->setSetting(GC_DROPBOX_TOKEN, access_token);
            QString info = QString(tr("Dropbox authorization was successful."));
            QMessageBox information(QMessageBox::Information, tr("Information"), info);
            information.exec();

        } else if (site == SPORTTRACKS) {

            service->setSetting(GC_SPORTTRACKS_TOKEN, access_token);
            service->setSetting(GC_SPORTTRACKS_REFRESH_TOKEN, refresh_token);
            service->setSetting(GC_SPORTTRACKS_LAST_REFRESH, QDateTime::currentDateTime());
            QString info = QString(tr("SportTracks authorization was successful."));
            QMessageBox information(QMessageBox::Information, tr("Information"), info);
            information.exec();

        } else if (site == POLAR) {

            service->setSetting(GC_POLARFLOW_TOKEN, access_token);
            service->setSetting(GC_POLARFLOW_USER_ID, polar_userid);

            // we now need to bind the user, this is a one time deal.
            QString url = QString("%1/v3/users").arg(GC_POLARFLOW_URL);

            // request using the bearer token
            QNetworkRequest request(url);
            request.setRawHeader("Authorization", (QString("Bearer %1").arg(access_token)).toLatin1());
            request.setRawHeader("Accept", "application/json");
            request.setRawHeader("Content-Type", "application/json");

            // data to post
            QByteArray data;
            data.append(QString("{\"member-id\":\"%1\"}").arg(context->athlete->cyclist).toUtf8());

            // the request will fallback to this method on networkRequestFinished
            // but we are done, so set ignore= true to get this function to just
            // return without doing anything
            ignore=true;
            QNetworkReply *bind = manager->post(request, data);

            // blocking request
            QEventLoop loop;
            connect(bind, SIGNAL(finished()), &loop, SLOT(quit()));
            loop.exec();

            // Bind response lists athlete details, we ignore them for now
            QByteArray r = bind->readAll();
            //qDebug()<<bind->errorString()<< "bind response="<<r;

            QString info = QString(tr("Polar Flow authorization was successful."));
            QMessageBox information(QMessageBox::Information, tr("Information"), info);
            information.exec();

        } else if (site == STRAVA) {

            service->setSetting(GC_STRAVA_TOKEN, access_token);
            service->setSetting(GC_STRAVA_REFRESH_TOKEN, refresh_token);
            service->setSetting(GC_STRAVA_LAST_REFRESH, QDateTime::currentDateTime());
            QString info = QString(tr("Strava authorization was successful."));
            QMessageBox information(QMessageBox::Information, tr("Information"), info);
            information.exec();

        } else if (site == CYCLING_ANALYTICS) {

            service->setSetting(GC_CYCLINGANALYTICS_TOKEN, access_token);
            QString info = QString(tr("Cycling Analytics authorization was successful."));
            QMessageBox information(QMessageBox::Information, tr("Information"), info);
            information.exec();

        } else if (site == XERT) {

            service->setSetting(GC_XERT_TOKEN, access_token);
            service->setSetting(GC_XERT_REFRESH_TOKEN, refresh_token);

            // Try without Message Box

            //QString info = QString(tr("Xert authorization was successful."));
            //QMessageBox information(QMessageBox::Information, tr("Information"), info);
            //information.exec();

            service->message = "Xert authorization was successful.";

        } else if (site == RIDEWITHGPS) {

            service->setSetting(GC_RWGPS_AUTH_TOKEN, access_token);

            service->message = "Ride With GPS authorization was successful.";

        } else if (site == WITHINGS) {

            service->setSetting(GC_NOKIA_TOKEN, access_token);
            service->setSetting(GC_NOKIA_REFRESH_TOKEN, refresh_token);

            // We have to ask for userid ?



            QString info = QString(tr("Withings authorization was successful."));
            QMessageBox information(QMessageBox::Information, tr("Information"), info);
            information.exec();

        } else if (site == NOLIO) {
            appsettings->setValue(GC_NOLIO_ACCESS_TOKEN, access_token);
            appsettings->setValue(GC_NOLIO_REFRESH_TOKEN, refresh_token);
            appsettings->setValue(GC_NOLIO_LAST_REFRESH, QDateTime::currentDateTime());
            QString info = QString(tr("Nolio authorization was successful."));
            QMessageBox information(QMessageBox::Information, tr("Information"), info);
            information.exec();
        } else if (site == AZUM) {
            service->setSetting(GC_AZUM_ACCESS_TOKEN, access_token);
            service->setSetting(GC_AZUM_REFRESH_TOKEN, refresh_token);
            QString info = QString(tr("Azum authorization was successful."));
            QMessageBox information(QMessageBox::Information, tr("Information"), info);
            information.exec();

        }

    } else {
            // general error getting response
            QString error = QString(tr("Error retrieving access token, %1 (%2)")).arg(reply->errorString()).arg(reply->error());
            QMessageBox oautherr(QMessageBox::Critical, tr("SSL Token Refresh Error"), error);
            oautherr.setDetailedText(error);
            oautherr.exec();
    }

    // job done, dialog can be closed
    accept();
}
