// Residual - Virtual machine to run LucasArts' 3D adventure games
// Copyright (C) 2003-2004 The ScummVM-Residual Team (www.scummvm.org)
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2.1 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with this library; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA

#include "../stdafx.h"
#include "../bits.h"
#include "../debug.h"
#include "../timer.h"

#include "../mixer/mixer.h"
#include "../mixer/audiostream.h"

#include "imuse_sndmgr.h"

void Imuse::parseScriptCmds(int cmd, int b, int c, int d, int e, int f, int g, int h) {
	int soundId = b;
	int sub_cmd = c;

	if (!cmd)
		return;

	switch (cmd) {
	case 10: // ImuseStopAllSounds
		stopAllSounds();
		break;
	case 12: // ImuseSetParam
		switch (sub_cmd) {
		case 0x400: // select group volume
			selectVolumeGroup(soundId, d);
			break;
		case 0x500: // set priority
			setPriority(soundId, d);
			break;
		case 0x600: // set volume
			setVolume(soundId, d);
			break;
		case 0x700: // set pan
			setPan(soundId, d);
			break;
		default:
			warning("Imuse::doCommand SetParam DEFAULT command %d", sub_cmd);
			break;
		}
		break;
	case 14: // ImuseFadeParam
		switch (sub_cmd) {
		case 0x600: // set volume fading
			if ((d != 0) && (e == 0))
				setVolume(soundId, d);
			else if ((d == 0) && (e == 0))
				stopSound(soundId);
			else
				setFade(soundId, d, e);
			break;
		default:
			warning("Imuse::doCommand FadeParam DEFAULT sub command %d", sub_cmd);
			break;
		}
		break;
	case 0x1000: // ImuseSetState
		debug(5, "ImuseSetState (%d)", b);
		setMusicState(b);
		break;
	case 0x1001: // ImuseSetSequence
		debug(5, "ImuseSetSequence (%d)", b);
		setMusicSequence(b);
		break;
	case 0x1002: // ImuseSetCuePoint
		debug(5, "ImuseSetCuePoint (%d)", b);
		break;
	case 0x1003: // ImuseSetAttribute
		debug(5, "ImuseSetAttribute (%d, %d)", b, c);
		_attributes[b] = c;
		break;
	case 0x2000: // ImuseSetGroupSfxVolume
		debug(5, "ImuseSetGroupSFXVolume (%d)", b);
//		setGroupSfxVolume(b);
		break;
	case 0x2001: // ImuseSetGroupVoiceVolume
		debug(5, "ImuseSetGroupVoiceVolume (%d)", b);
//		setGroupVoiceVolume(b);
		break;
	case 0x2002: // ImuseSetGroupMusicVolume
		debug(5, "ImuseSetGroupMusicVolume (%d)", b);
//		setGroupMusicVolume(b);
		break;
	default:
		error("Imuse::doCommand DEFAULT command %d", cmd);
	}
}

void Imuse::flushTracks() {
	debug(5, "flushTracks()");
	for (int l = 0; l < MAX_DIGITAL_TRACKS + MAX_DIGITAL_FADETRACKS; l++) {
		Track *track = _track[l];
		if (track->used && track->readyToRemove) {
			if (track->stream) {
				if (!track->stream->endOfStream()) {
	 				track->stream->finish();
	 			}
				if (track->stream->endOfStream()) {
					_vm->_mixer->stopHandle(track->handle);
					delete track->stream;
					track->stream = NULL;
					_sound->closeSound(track->soundHandle);
					track->soundHandle = NULL;
					track->used = false;
				}
			}
		}
	}
}

void Imuse::refreshScripts() {
	debug(5, "refreshScripts()");
	bool found = false;
	for (int l = 0; l < MAX_DIGITAL_TRACKS; l++) {
		Track *track = _track[l];
		if (track->used && !track->toBeRemoved && (track->volGroupId == IMUSE_VOLGRP_MUSIC)) {
			found = true;
		}
	}

	if (!found && (_curMusicSeq != 0)) {
		debug(5, "refreshScripts() Start Sequence");
		parseScriptCmds(0x1001, 0, 0, 0, 0, 0, 0, 0);
	}
}

void Imuse::startVoice(const char *soundName, int soundId) {
	debug(5, "startVoiceBundle(%s)", soundName);
	startSound(soundId, soundName, IMUSE_VOLGRP_VOICE, NULL, 0, 127, 127);
}

void Imuse::startMusic(const char *soundName, int soundId, int hookId, int volume, int pan) {
	debug(5, "startMusicBundle(%s)", soundName);
	startSound(soundId, soundName, IMUSE_VOLGRP_MUSIC, NULL, hookId, volume, pan, 126);
}

void Imuse::startSfx(const char *soundName, int soundId, int priority) {
	debug(5, "startSfx(%d)", soundId);
	startSound(soundId, soundName, IMUSE_VOLGRP_SFX, NULL, 0, 127, priority);
}

int32 Imuse::getPosInMs(int soundId) {
	for (int l = 0; l < MAX_DIGITAL_TRACKS; l++) {
		Track *track = _track[l];
		if ((track->soundId == soundId) && track->used && !track->toBeRemoved) {
			int32 pos = (5 * (track->dataOffset + track->regionOffset)) / (track->iteration / 200);
			return pos;
		}
	}

	return 0;
}

bool Imuse::getSoundStatus(int sound) const {
	debug(5, "Imuse::getSoundStatus(%d)", sound);
	for (int l = 0; l < MAX_DIGITAL_TRACKS; l++) {
		Track *track = _track[l];
		if (track->soundId == sound) {
			if (track->handle.isActive() || (track->stream && track->used && !track->readyToRemove)) {
					return true;
			}
		}
	}

	return false;
}

void Imuse::stopSound(int soundId) {
	debug(5, "Imuse::stopSound(%d)", soundId);
	for (int l = 0; l < MAX_DIGITAL_TRACKS; l++) {
		Track *track = _track[l];
		if ((track->soundId == soundId) && track->used && !track->toBeRemoved) {
			track->toBeRemoved = true;
		}
	}
}

int32 Imuse::getCurMusicPosInMs() {
	int soundId = -1;

	for (int l = 0; l < MAX_DIGITAL_TRACKS; l++) {
		Track *track = _track[l];
		if (track->used && !track->toBeRemoved && (track->volGroupId == IMUSE_VOLGRP_MUSIC)) {
			soundId = track->soundId;
		}
	}

	int32 msPos = getPosInMs(soundId);
	debug(5, "Imuse::getCurMusicPosInMs(%d) = %d", soundId, msPos);
	return msPos;
}

void Imuse::stopAllSounds() {
	debug(5, "Imuse::stopAllSounds");

	for(;;) {
		bool foundNotRemoved = false;
		for (int l = 0; l < MAX_DIGITAL_TRACKS + MAX_DIGITAL_FADETRACKS; l++) {
			Track *track = _track[l];
			if (track->used) {
				track->toBeRemoved = true;
				foundNotRemoved = true;
			}
		}
		if (!foundNotRemoved)
			break;
		flushTracks();
		SDL_Delay(50);
	}
}

void Imuse::pause(bool p) {
	_pause = p;
}
