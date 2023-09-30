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
#include "AudioFx.h"

AudioFx::AudioFx() {
    m_thread.reset(new QThread);
    moveToThread(m_thread.get());
    m_thread->start();

    uploaded.setSource(QUrl::fromLocalFile(":/ConkorsCompanion/assets/upload.wav"));
    startedLive.setSource(QUrl::fromLocalFile(":/ConkorsCompanion/assets/startlive.wav"));
    stoppedLive.setSource(QUrl::fromLocalFile(":/ConkorsCompanion/assets/stoplive.wav"));
    savedReplay.setSource(QUrl::fromLocalFile(":/ConkorsCompanion/assets/savereplay.wav"));

    connect(this, &AudioFx::notifyUploaded, this, &AudioFx::playUploadedInternal, Qt::QueuedConnection);
    connect(this, &AudioFx::notifyStartedLive, this, &AudioFx::playStartedLiveInternal, Qt::QueuedConnection);
    connect(this, &AudioFx::notifyStoppedLive, this, &AudioFx::playStoppedLiveInternal, Qt::QueuedConnection);
    connect(this, &AudioFx::notifySavedReplay, this, &AudioFx::playSavedReplayInternal, Qt::QueuedConnection);
}

AudioFx::~AudioFx() {
    QMetaObject::invokeMethod(this, "cleanup");
    m_thread->wait();
}

void AudioFx::playUploaded() {
    emit notifyUploaded();
}
void AudioFx::playStartedLive() {
    emit notifyStartedLive();
}
void AudioFx::playStoppedLive() {
    emit notifyStoppedLive();
}
void AudioFx::playSavedReplay() {
    emit notifySavedReplay();
}

void AudioFx::playUploadedInternal() {
    uploaded.play();
}
void AudioFx::playStartedLiveInternal() {
    startedLive.play();
}
void AudioFx::playStoppedLiveInternal() {
    stoppedLive.play();
}
void AudioFx::playSavedReplayInternal() {
    savedReplay.play();
}

void AudioFx::cleanup() {
    m_thread->quit();
}
