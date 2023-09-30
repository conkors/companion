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

#include "stdafx.h"
#include "webapi.h"
#include "ConkorsCompanion.h"

#define LOGIN_PAGE_IDX 0
#define MAIN_PAGE_IDX 1
#define SETTINGS_PAGE_IDX 2

ConkorsCompanion::ConkorsCompanion(
        std::shared_ptr<AccountManager> accountManager,
        std::shared_ptr<VidCore> vidCore,
        std::shared_ptr<ImgCore> imgCore,
		std::string auth,
        QWidget *parent)
    : QWidget(parent), 
    mgr(new QNetworkAccessManager()), 
    oauth(new OAuthWorker()),
    sfx(new AudioFx()) {

    accMgr = accountManager;
    vc = vidCore;
    ic = imgCore;

    ui.setupUi(this);

    // Initial Default UI Configs
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setFixedSize(QSize(380, 500));
    ui.stack->setCurrentIndex(0);
	ui.displayCaptureButton->setChecked(true);
    ui.checkLabel->setHidden(true);

    /* SIGNAL HANDLERS */

    /// PAGE CHANGE
    connect(ui.stack, &QStackedWidget::currentChanged, this, &ConkorsCompanion::onPageChanged);
    /// SIGN IN CLICK
    connect(ui.signInButton, &QPushButton::clicked, this, &ConkorsCompanion::signInPressed);
    connect(ui.emailLineEdit, &QLineEdit::returnPressed, this, &ConkorsCompanion::signInPressed);
    connect(ui.passwordLineEdit, &QLineEdit::returnPressed, this, &ConkorsCompanion::signInPressed);
    /// SIGN UP CLICK
    connect(ui.registerButton, &QPushButton::clicked, [this]() {
		QString link = "https://conkors.com/register";
		QDesktopServices::openUrl(QUrl(link));
	});
    /// DISCORD CLICK
    connect(ui.discordButton, &QPushButton::clicked, [this]() {
		QDesktopServices::openUrl(QUrl(QString::fromStdString(DISCORD_OAUTH_LINK)));
	});
    /// SIGN OUT CLICK
    connect(ui.signOutButton, &QPushButton::clicked, this, &ConkorsCompanion::signOutPressed);

    /// MINIMIZE CLICK
    connect(ui.minButton, &QPushButton::clicked, this, &ConkorsCompanion::minimizePressed);

    /// CAPTURE MODE CHANGE
    connect(ui.displayCaptureButton, &QRadioButton::toggled, this, &ConkorsCompanion::onCaptureModeChanged);
    /// DISPLAY SELECTION CHANGE
    connect(ui.displayBox, &QComboBox::activated, this, &ConkorsCompanion::onDisplayChanged);
    /// SPEAKER SELECTION CHANGE
    connect(ui.speakerBox, &QComboBox::activated, this, &ConkorsCompanion::onSpeakerChanged);
    /// SPEAKER MUTE CHANGE
    connect(ui.muteSpeakerToggle, &QCheckBox::stateChanged, this, &ConkorsCompanion::onSpeakerMuteToggled);
    /// MIC SELECTION CHANGE
    connect(ui.micBox, &QComboBox::activated, this, &ConkorsCompanion::onMicChanged);
    /// MIC MUTE CHANGE
    connect(ui.muteMicToggle, &QCheckBox::stateChanged, this, &ConkorsCompanion::onMicMuteToggled);
    /// SETTINGS CLICK
    connect(ui.settingsButton, &QPushButton::clicked, this, &ConkorsCompanion::settingsPressed);
    /// ADAPTER CHANGE
    connect(ui.gfxCardBox, &QComboBox::activated, this, &ConkorsCompanion::onAdapterChanged);
    /// FORCE SOFTWARE CHANGE
    connect(ui.forceSoftwareButton, &QPushButton::clicked, this, &ConkorsCompanion::onForceSoftwareToggled);
    /// EXIT SETTINGS CLICK
    connect(ui.settingsExitButton, &QPushButton::clicked, this, &ConkorsCompanion::exitSettingsPressed);

    QSettings settings;
    QString authToken = settings.value("AuthToken").toString();
    bool prevRun = settings.value("PreviouslyRun").toBool();
    QString savedPath = settings.value("AppPath").toString();

    QString latestAppPath = getAppPath();
    // Initialize on very first run
	if (!prevRun || savedPath.isEmpty() || latestAppPath != savedPath) {
        registerUriScheme(latestAppPath.toStdString());
		settings.setValue("PreviouslyRun", true);
		settings.setValue("AppPath", latestAppPath);
    }

    if (!auth.empty()) {
		// Auth via args takes precedence
		// Save to settings, can be revoked if network request fails
        settings.setValue("AuthToken", QString::fromStdString(auth));
        session = authToken;
		ui.stack->setCurrentWidget(ui.mainPage);
    }
    else if (!authToken.isEmpty()) {
		// If auth token exists, skip the login page and proceed to the main menu
        session = authToken;
		ui.stack->setCurrentWidget(ui.mainPage);
	}
	else {
		ui.stack->setCurrentWidget(ui.loginPage);
	}

    initTray();
    initOAuthListener();
    initAudio();
}

ConkorsCompanion::~ConkorsCompanion()
{}

void ConkorsCompanion::closeEvent(QCloseEvent* event)
{
    if (quitEvent) {
        event->accept();
    }
    else {
		this->hide();
		event->ignore();
    }
}

void ConkorsCompanion::mousePressEvent(QMouseEvent* event) {
    // Check if mouse press is within the top 30 pixels to drag
    if (event->button() == Qt::LeftButton && event->y() <= 30) {
        isDragging = true;
        dragStart = event->globalPosition().toPoint();
    }
}

void ConkorsCompanion::mouseReleaseEvent(QMouseEvent* event) {
    isDragging = false;
}

void ConkorsCompanion::mouseMoveEvent(QMouseEvent* event) {
    if (isDragging) {
        dragEnd = QPoint(event->globalPosition().toPoint() - dragStart);
        move(x() + dragEnd.x(), y() + dragEnd.y());
        dragStart = event->globalPosition().toPoint();
    }
}

/// SIGNAL HANDLING

void ConkorsCompanion::signInPressed() {
    // Reset error
    ui.loginErrorLabel->setHidden(true);
    ui.loginErrorLabel->setText("");

    QString email = ui.emailLineEdit->text();
    QString password = ui.passwordLineEdit->text();

    // Check Validity
    if (!isValidEmail(email)) {
		ui.loginErrorLabel->setText("Please enter a valid email");
		ui.loginErrorLabel->setHidden(false);
        return;
    }
    if (password.length() < 8) {
		ui.loginErrorLabel->setText("Please enter a valid password");
		ui.loginErrorLabel->setHidden(false);
        return;
    }

    requestSignIn(email, password);
}

void ConkorsCompanion::signInResponded() {
    QString token;

    // Parse the network reply
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender()); // Get the sender
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray responseData = reply->readAll();

        // Parse the JSON response
        QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
        if (!jsonDoc.isNull() && jsonDoc.isObject()) {
            QJsonObject jsonObject = jsonDoc.object();
            // Check if the "auth" key exists
            if (jsonObject.contains("auth") && jsonObject["auth"].isString()) {
                QString authToken = jsonObject["auth"].toString();
                token = authToken;
            }
        }
    }
    reply->deleteLater();


    if (!token.isEmpty()) {
        session = token;

		// Save to settings
        QSettings settings;
		settings.setValue("AuthToken", token);

		// Update UI
		ui.stack->setCurrentWidget(ui.mainPage);
    }
    else {
        // Show login failed error
		ui.loginErrorLabel->setHidden(false);
		ui.loginErrorLabel->setText("Failed to login, check credentials");
    }
}

void ConkorsCompanion::tokenReceived(const QString& authToken) {
	session = authToken;

	// Save to settings
    QSettings settings;
	settings.setValue("AuthToken", authToken);

	// Update UI
	ui.stack->setCurrentWidget(ui.mainPage);
	ui.emailLineEdit->setText("");
	ui.passwordLineEdit->setText("");
}

void ConkorsCompanion::userInfoResponded() {
    QString handle;
    QString avatar;
    bool isPremium = false;

    // Parse the network reply
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender()); // Get the sender
	if (reply->error() == QNetworkReply::NoError) {
		QByteArray responseData = reply->readAll();

		// Parse the JSON response
		QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
		if (!jsonDoc.isNull() && jsonDoc.isObject()) {
			QJsonObject jsonObject = jsonDoc.object();
			// Check if valid
			if (jsonObject.contains("handle") && jsonObject["handle"].isString()
				&& jsonObject.contains("avatar") && jsonObject["avatar"].isString()
				&& jsonObject.contains("is_premium") && jsonObject["is_premium"].isBool()) {

				handle = jsonObject["handle"].toString();
				avatar = jsonObject["avatar"].toString();
				isPremium = jsonObject["is_premium"].toBool();
			}
		}
	}
	reply->deleteLater();

	if (!handle.isEmpty()) {
        // Core Auth
        accMgr->setSessionToken(session.toStdString());

		// Update UI
        ui.handleLabel->setText(handle);
        if (!avatar.isEmpty()) {
            requestAvatar(avatar);
        }
        if (isPremium) {
            ui.checkLabel->setHidden(false);
        }
	}
	else {
        // Bail out, kill session
        signOutPressed();
	}
}

void ConkorsCompanion::avatarResponded() {
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender()); // Get the sender
	if (reply->error() == QNetworkReply::NoError) {
		// Read the downloaded image bytes
		QByteArray imageData = reply->readAll();

		// Create a QPixmap from the image bytes
		QPixmap p;
		p.loadFromData(imageData);
		p = p.scaled(100, 100, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);

		// Create a QPixmap for the circular avatar
		QPixmap circularAvatar(100, 100);
		circularAvatar.fill(Qt::transparent);

		QPainter painter(&circularAvatar);
		painter.setRenderHint(QPainter::Antialiasing, true);
		painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

		// Create a circular path
		QPainterPath path;
		path.addEllipse(0, 0, 100, 100);
		painter.setClipPath(path);

		// Draw the scaled image onto the circular QPixmap
		painter.drawPixmap(0, 0, p);

		// Set the circular QPixmap as the pixmap for the QLabel
		ui.avatarLabel->setPixmap(circularAvatar);
	} 

	reply->deleteLater();
}

void ConkorsCompanion::signOutPressed() {
    initialized = false;
    session = QString();
    accMgr->deleteSessionToken();

	// Clear the token from settings when the user logs out
    QSettings settings;
	settings.remove("AuthToken");

    // Change View
	ui.stack->setCurrentWidget(ui.loginPage);
}

void ConkorsCompanion::minimizePressed() {
    hide();
}

void ConkorsCompanion::onPageChanged(int index) {
    switch (index)
    {
    case LOGIN_PAGE_IDX:
        handleShowLoginPage();
        break;
    case MAIN_PAGE_IDX:
        handleShowMainPage();
        break;
    case SETTINGS_PAGE_IDX:
        handleShowSettingsPage();
        break;
    default:
        break;
    }
}

void ConkorsCompanion::onCaptureModeChanged() {
    if (ui.displayCaptureButton->isChecked()) {
        vc->stopReplay();
        vc->saveLive();
        vc->captureMonitor();
    }
    else if (ui.windowCaptureButton->isChecked()) {
        vc->stopReplay();
        vc->saveLive();
        vc->captureWindow();
    }
    populateDisplayOptions();
    vc->recordReplay();
}

void ConkorsCompanion::onDisplayChanged() {
	vc->stopReplay();
	vc->saveLive();

    QVariant data = ui.displayBox->currentData();
    QString dataStr = data.toString();
    std::string dispId = dataStr.toStdString();
    vc->updateDisplay(dispId.c_str());
    vc->recordReplay();
}

void ConkorsCompanion::onSpeakerChanged() {
    QVariant data = ui.speakerBox->currentData();
    QString dataStr = data.toString();
    std::string spkId = dataStr.toStdString();
    vc->updateSpeaker(spkId.c_str());
}

void ConkorsCompanion::onAdapterChanged() {
    QVariant data = ui.gfxCardBox->currentData();
    int idx = data.toInt();
    vc->overrideAdapter(idx);
    initialized = false;
}

void ConkorsCompanion::onForceSoftwareToggled() {
    ui.forceSoftwareButton->setDisabled(true);
    ui.forceSoftwareButton->setText("Software Encoder On");
	vc->forceSoftwareEncoder();
    initialized = false;
}

void ConkorsCompanion::onSpeakerMuteToggled() {
    if (ui.muteSpeakerToggle->isChecked()) {
        vc->toggleSpeakerAudio(true);
    }
    else {
        vc->toggleSpeakerAudio(false);
    }
}

void ConkorsCompanion::onMicChanged() {
    QVariant data = ui.micBox->currentData();
    QString dataStr = data.toString();
    std::string micId = dataStr.toStdString();
    vc->updateMic(micId.c_str());
}

void ConkorsCompanion::onMicMuteToggled() {
    if (ui.muteMicToggle->isChecked()) {
        vc->toggleMicAudio(true);
    }
    else {
        vc->toggleMicAudio(false);
    }
}

void ConkorsCompanion::settingsPressed() {
	ui.stack->setCurrentWidget(ui.settingsPage);
}

void ConkorsCompanion::exitSettingsPressed() {
	ui.stack->setCurrentWidget(ui.mainPage);
}

/// HELPERS

void ConkorsCompanion::requestSignIn(const QString& email, const QString& password) {
    if (email.isEmpty() || password.isEmpty()) return;

    const QUrl url(URL_LOGIN);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject obj;
    obj["email"] = email;
    obj["password"] = password;
    QJsonDocument doc(obj);
    QByteArray data = doc.toJson();

    QNetworkReply* reply = mgr->post(request, data);
    connect(reply, &QNetworkReply::finished, this, &ConkorsCompanion::signInResponded);
}

void ConkorsCompanion::requestUserInfo() {
	if (session.isEmpty()) return;

	const QUrl url(URL_PP);
	QNetworkRequest request(url);
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

	QString cookieName = AUTH_COOKIE;
	QNetworkCookie cookie(cookieName.toUtf8(), session.toUtf8());
	QByteArray cookieHeader = cookie.toRawForm();
	request.setRawHeader("Cookie", cookieHeader);

	QNetworkReply* reply = mgr->get(request);
    connect(reply, &QNetworkReply::finished, this, &ConkorsCompanion::userInfoResponded);
}

void ConkorsCompanion::requestAvatar(const QString& avatarUrl) {
    if (avatarUrl.isEmpty()) return;

	QNetworkReply* reply = mgr->get(QNetworkRequest(QUrl(avatarUrl)));
    connect(reply, &QNetworkReply::finished, this, &ConkorsCompanion::avatarResponded);
}

void ConkorsCompanion::handleShowLoginPage() {
	ui.emailLineEdit->setText("");
	ui.passwordLineEdit->setText("");
}

void ConkorsCompanion::handleShowMainPage() {
	if (session.isEmpty()) {
		// Main page with no session??
		ui.stack->setCurrentWidget(ui.loginPage);
		return;
	}

	// Data fetching only needs to happen once
    if (!initialized) {
        vc->toggleSpeakerAudio(false);
        vc->toggleMicAudio(false);

		requestUserInfo();

		// Grab all the props for combo boxes
		populateDisplayOptions();
		populateSpeakerOptions();
		populateMicOptions();

        initialized = true;
    }
}

void ConkorsCompanion::handleShowSettingsPage() {
	populateAdapters();
}

void ConkorsCompanion::populateAdapters() {
    ui.gfxCardBox->clear();
    propListInt adapters = vc->getAdapters();
	for (auto& a : adapters) {
        ui.gfxCardBox->addItem(QString::fromStdString(a.first), a.second);
	}
}

void ConkorsCompanion::populateDisplayOptions() {
    ui.displayBox->clear();
    propListStr displays = vc->getDisplayOpts();
	for (auto& disp : displays) {
        ui.displayBox->addItem(QString::fromStdString(disp.first), QString::fromStdString(disp.second));
	}
}

void ConkorsCompanion::populateSpeakerOptions() {
    ui.speakerBox->clear();
    propListStr speakers = vc->getSpeakerOpts();
	for (auto& spk : speakers) {
        ui.speakerBox->addItem(QString::fromStdString(spk.first), QString::fromStdString(spk.second));
	}
}

void ConkorsCompanion::populateMicOptions() {
    ui.micBox->clear();
    propListStr mics = vc->getMicOpts();
	for (auto& m : mics) {
        ui.micBox->addItem(QString::fromStdString(m.first), QString::fromStdString(m.second));
	}
}

void ConkorsCompanion::registerUriScheme(const std::string& appPath) {
	const char* uriScheme = "ConkorsCompanion";

	// Create the registry keys
	HKEY hKeyUriScheme = NULL;
	HKEY hKeyShellOpenCommand = NULL;

	// Create or open the key for the custom URI scheme
	if (RegCreateKeyExA(HKEY_CLASSES_ROOT, uriScheme, 0, NULL, 0, KEY_WRITE, NULL, &hKeyUriScheme, NULL) == ERROR_SUCCESS) {
		// Set the default value for the URI scheme
		RegSetValueExA(hKeyUriScheme, NULL, 0, REG_SZ, (const BYTE*)"URL:Conkors Companion Protocol", sizeof("URL:Conkors Companion Protocol"));

		// Set the URL Protocol value
		RegSetValueExA(hKeyUriScheme, "URL Protocol", 0, REG_SZ, (const BYTE*)"", sizeof(""));

		// Create or open the key for the shell\open\command subkey
		if (RegCreateKeyExA(hKeyUriScheme, "shell\\open\\command", 0, NULL, 0, KEY_WRITE, NULL, &hKeyShellOpenCommand, NULL) == ERROR_SUCCESS) {
			// Set the default value for the open command
			char openCommand[MAX_PATH];
			sprintf_s(openCommand, "%s %%1", appPath.c_str());

			RegSetValueExA(hKeyShellOpenCommand, NULL, 0, REG_SZ, (const BYTE*)openCommand, sizeof(openCommand));

			// Close the open command key
			RegCloseKey(hKeyShellOpenCommand);
		}

		// Close the URI scheme key
		RegCloseKey(hKeyUriScheme);
	}
	else {
		printf("CK::UI FAILED TO REGISTER APP URI!!!\n");
	}
}

void ConkorsCompanion::initTray() {
    QAction* exitAction = new QAction(tr("&Exit"), this);
    // Handle exit from tray icon
    connect(exitAction, &QAction::triggered, [this]() {
        // Closing for reals
        quitEvent = true;
		close();
	});

    QMenu* trayIconMenu = new QMenu(this);
    trayIconMenu->addAction(exitAction);

    QSystemTrayIcon* sysTrayIcon = new QSystemTrayIcon(this);
    sysTrayIcon->setContextMenu(trayIconMenu);
    sysTrayIcon->setIcon(QIcon(":/ConkorsCompanion/assets/img/tray.ico"));
    sysTrayIcon->show();

    // Toggle show/hide window from tray icon
    connect(sysTrayIcon, &QSystemTrayIcon::activated, [this](auto reason) {
		if (reason == QSystemTrayIcon::Trigger) {
			if (isVisible()) {
				hide();
			}
			else {
				show();
				activateWindow();
			}
		}
	});
}

void ConkorsCompanion::initOAuthListener() {
    connect(oauth, &OAuthWorker::tokenReceived, this, &ConkorsCompanion::tokenReceived);
    oauth->listenForToken();
}

void ConkorsCompanion::initAudio() {
    accMgr->attachUploadedSfx(std::bind(&AudioFx::playUploaded, sfx));
    accMgr->attachStartLiveSfx(std::bind(&AudioFx::playStartedLive, sfx));
    accMgr->attachStopLiveSfx(std::bind(&AudioFx::playStoppedLive, sfx));
    accMgr->attachSaveReplaySfx(std::bind(&AudioFx::playSavedReplay, sfx));
}

bool ConkorsCompanion::isValidEmail(const QString& email) {
    QRegularExpression emailRegex("^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\\.[A-Za-z]{2,4}$");
    QRegularExpressionMatch match = emailRegex.match(email);
    return match.hasMatch();
}

QString ConkorsCompanion::getAppPath() {
	char buffer[MAX_PATH];
	GetModuleFileNameA(NULL, buffer, MAX_PATH);
	std::string appPath(buffer);
    return QString::fromStdString(appPath);
}
