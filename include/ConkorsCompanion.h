/******************************************************************************
	CONKORS COMPANION
	www.conkors.com
	Copyright (C) Conkors LLC

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#pragma once

#include <future>
#include <QtWidgets/QWidget>
#include "ui_ConkorsCompanion.h"
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QAuthenticator>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkCookie>
#include <QJsonObject>
#include <QJsonDocument>
#include <QByteArray>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include "OAuthWorker.h"
#include "AudioFx.h"

#include "account.h"
#include "vid.h"
#include "img.h"

class ConkorsCompanion : public QWidget
{
    Q_OBJECT

public:
    ConkorsCompanion(
        std::shared_ptr<AccountManager> accountManager,
        std::shared_ptr<VidCore> vidCore,
        std::shared_ptr<ImgCore> imgCore,
        std::string auth,
        QWidget *parent = nullptr);
    ~ConkorsCompanion();

private:
    Ui::ConkorsCompanionClass ui;

    std::shared_ptr<AccountManager> accMgr;
    std::shared_ptr<VidCore> vc;
    std::shared_ptr<ImgCore> ic;

    bool initialized;
    bool quitEvent;
    bool isDragging;

    QPoint dragStart;
    QPoint dragEnd;

    QString session;

    QNetworkAccessManager* mgr;
    OAuthWorker* oauth;
    AudioFx* sfx;

    void closeEvent(QCloseEvent* event);
    void mousePressEvent(QMouseEvent* event);
    void mouseReleaseEvent(QMouseEvent* event);
    void mouseMoveEvent(QMouseEvent* event);

    ///  SLOTS (SIGNAL HANDLERS)
    void signInPressed();
    void signInResponded();
    void tokenReceived(const QString& authToken);
    void userInfoResponded();
    void avatarResponded();
    void signOutPressed();
    void minimizePressed();
    void onPageChanged(int index);
    void onCaptureModeChanged();
    void onAdapterChanged();
    void onForceSoftwareToggled();
    void onDisplayChanged();
    void onSpeakerChanged();
    void onSpeakerMuteToggled();
    void onMicChanged();
    void onMicMuteToggled();
    void settingsPressed();
    void exitSettingsPressed();

    ///  HELPERS
    void requestSignIn(const QString& email, const QString& password);
    void requestUserInfo();
    void requestAvatar(const QString& avatarUrl);
    void handleShowLoginPage();
    void handleShowMainPage();
    void handleShowSettingsPage();

    void populateAdapters();
    void populateDisplayOptions();
    void populateSpeakerOptions();
    void populateMicOptions();

    void initTray();
    void initOAuthListener();
    void initAudio();

    void registerUriScheme(const std::string& appPath);

    bool isValidEmail(const QString& email);

    QString getAppPath();
};
