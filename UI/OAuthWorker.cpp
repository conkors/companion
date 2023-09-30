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
#include "OAuthWorker.h"
#include <QUrl>
#include <QUrlQuery>

OAuthWorker::OAuthWorker() {
    m_thread.reset(new QThread);
    moveToThread(m_thread.get());
    m_thread->start();

    // Connect this method to run on the worker thread using a queued connection
    connect(this, &OAuthWorker::start, this, &OAuthWorker::doWork, Qt::QueuedConnection);
}

OAuthWorker::~OAuthWorker() {
    QMetaObject::invokeMethod(this, "cleanup");
    m_thread->wait();
}

void OAuthWorker::listenForToken() {
    // Emit a signal to trigger the listening on worker thread
    emit start();
}

void OAuthWorker::doWork() {
    HANDLE hPipe = CreateNamedPipeA("\\\\.\\pipe\\ConkorsCompanion", PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 64, 64, 0, NULL);

    int bufferSize = 1024;
    char* buffer = new char[bufferSize];
    std::memset(buffer, 0, bufferSize);

    while (1) {
        if (ConnectNamedPipe(hPipe, NULL)) {
            // Read the authentication token from the pipe
            ReadFile(hPipe, buffer, bufferSize, NULL, NULL);

            // Parse the URL
            std::string str(buffer);
            QUrl url(QString::fromStdString(str));

            // Extract the 'auth' parameter
            QString authValue;
            QUrlQuery urlQuery(url.query());
            if (urlQuery.hasQueryItem("auth")) {
                authValue = urlQuery.queryItemValue("auth");
            }

            if (!authValue.isEmpty()) {
                emit tokenReceived(authValue);
            }

            std::memset(buffer, 0, bufferSize);
            // Disconnect the pipe and continue listening for the next token
            DisconnectNamedPipe(hPipe);
        }
    }
}

void OAuthWorker::cleanup() {
    m_thread->quit();
}
