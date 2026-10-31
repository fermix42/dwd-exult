/*
 * Copyright (C) 2001  Ludvig Strigeus
 * Copyright (C) 2001/2002 The ScummVM project
 * Copyright (C) 2002/2005-2022 The Exult Team
 * Copyright (C) 2003 The Pentagram Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "pent_include.h"

#ifdef USE_FMOPL_MIDI
#	include "Opl3StereoMidiDriver.h"
// Independent adaptation of Exult's GPL FMOpl instrument/voice handling.
// Keep the verified FMOpl implementation unchanged; this backend uses Nuked.

#	include "XMidiEvent.h"
#	include "databuf.h"
#	include "utils.h"
#	include "sha1/sha1.h"
#	include <stdexcept>

#	include <algorithm>
#	include <cmath>

const MidiDriver::MidiDriverDesc Opl3StereoMidiDriver::desc = MidiDriver::MidiDriverDesc("OPL3Stereo", createInstance);

// #define LUCAS_MODE

// We fudge with the velocity. If it's 16, it complies to AIL 2.0 Specs
#	define VEL_FUDGE 2

/* This is the internal emulated MIDI driver using the included OPL2 sound chip
 * FM instrument definitions below borrowed from the Allegro library by
 * Phil Hassey, <philhassey@hotmail.com> (www.imitationpickles.org)
 */

const Opl3StereoMidiDriver::xinstrument Opl3StereoMidiDriver::midi_fm_instruments_table[128] = {
		/* This set of GM instrument patches was provided by Jorrit
		 * Rouwe...
		 */
		{0x21, 0x21, 0x8f, 0x0c, 0xf2, 0xf2, 0x45, 0x76, 0x00, 0x00, 0x08, 0x80}, /* Acoustic Grand */
		{0x31, 0x21, 0x4b, 0x09, 0xf2, 0xf2, 0x54, 0x56, 0x00, 0x00, 0x08, 0x80}, /* Bright Acoustic */
		{0x31, 0x21, 0x49, 0x09, 0xf2, 0xf2, 0x55, 0x76, 0x00, 0x00, 0x08, 0x80}, /* Electric Grand */
		{0xb1, 0x61, 0x0e, 0x09, 0xf2, 0xf3, 0x3b, 0x0b, 0x00, 0x00, 0x06, 0x80}, /* Honky-Tonk */
		{0x01, 0x21, 0x57, 0x09, 0xf1, 0xf1, 0x38, 0x28, 0x00, 0x00, 0x00, 0x80}, /* Electric Piano 1 */
		{0x01, 0x21, 0x93, 0x09, 0xf1, 0xf1, 0x38, 0x28, 0x00, 0x00, 0x00, 0x80}, /* Electric Piano 2 */
		{0x21, 0x36, 0x80, 0x17, 0xa2, 0xf1, 0x01, 0xd5, 0x00, 0x00, 0x08, 0x80}, /* Harpsichord */
		{0x01, 0x01, 0x92, 0x09, 0xc2, 0xc2, 0xa8, 0x58, 0x00, 0x00, 0x0a, 0x80}, /* Clav */
		{0x0c, 0x81, 0x5c, 0x09, 0xf6, 0xf3, 0x54, 0xb5, 0x00, 0x00, 0x00, 0x80}, /* Celesta */
		{0x07, 0x11, 0x97, 0x89, 0xf6, 0xf5, 0x32, 0x11, 0x00, 0x00, 0x02, 0x80}, /* Glockenspiel */
		{0x17, 0x01, 0x21, 0x09, 0x56, 0xf6, 0x04, 0x04, 0x00, 0x00, 0x02, 0x80}, /* Music Box */
		{0x18, 0x81, 0x62, 0x09, 0xf3, 0xf2, 0xe6, 0xf6, 0x00, 0x00, 0x00, 0x80}, /* Vibraphone */
		{0x18, 0x21, 0x23, 0x09, 0xf7, 0xe5, 0x55, 0xd8, 0x00, 0x00, 0x00, 0x80}, /* Marimba */
		{0x15, 0x01, 0x91, 0x09, 0xf6, 0xf6, 0xa6, 0xe6, 0x00, 0x00, 0x04, 0x80}, /* Xylophone */
		{0x45, 0x81, 0x59, 0x89, 0xd3, 0xa3, 0x82, 0xe3, 0x00, 0x00, 0x0c, 0x80}, /* Tubular Bells */
		{0x03, 0x81, 0x49, 0x89, 0x74, 0xb3, 0x55, 0x05, 0x01, 0x00, 0x04, 0x80}, /* Dulcimer */
		{0x71, 0x31, 0x92, 0x09, 0xf6, 0xf1, 0x14, 0x07, 0x00, 0x00, 0x02, 0x80}, /* Drawbar Organ */
		{0x72, 0x30, 0x14, 0x09, 0xc7, 0xc7, 0x58, 0x08, 0x00, 0x00, 0x02, 0x80}, /* Percussive Organ */
		{0x70, 0xb1, 0x44, 0x09, 0xaa, 0x8a, 0x18, 0x08, 0x00, 0x00, 0x04, 0x80}, /* Rock Organ */
		{0x23, 0xb1, 0x93, 0x09, 0x97, 0x55, 0x23, 0x14, 0x01, 0x00, 0x04, 0x80}, /* Church Organ */
		{0x61, 0xb1, 0x13, 0x89, 0x97, 0x55, 0x04, 0x04, 0x01, 0x00, 0x00, 0x80}, /* Reed Organ */
		{0x24, 0xb1, 0x48, 0x09, 0x98, 0x46, 0x2a, 0x1a, 0x01, 0x00, 0x0c, 0x80}, /* Accoridan */
		{0x61, 0x21, 0x13, 0x09, 0x91, 0x61, 0x06, 0x07, 0x01, 0x00, 0x0a, 0x80}, /* Harmonica */
		{0x21, 0xa1, 0x13, 0x92, 0x71, 0x61, 0x06, 0x07, 0x00, 0x00, 0x06, 0x80}, /* Tango Accordian */
		{0x02, 0x41, 0x9c, 0x89, 0xf3, 0xf3, 0x94, 0xc8, 0x01, 0x00, 0x0c, 0x80}, /* Acoustic Guitar(nylon) */
		{0x03, 0x11, 0x54, 0x09, 0xf3, 0xf1, 0x9a, 0xe7, 0x01, 0x00, 0x0c, 0x80}, /* Acoustic Guitar(steel) */
		{0x23, 0x21, 0x5f, 0x09, 0xf1, 0xf2, 0x3a, 0xf8, 0x00, 0x00, 0x00, 0x80}, /* Electric Guitar(jazz) */
		{0x03, 0x21, 0x87, 0x89, 0xf6, 0xf3, 0x22, 0xf8, 0x01, 0x00, 0x06, 0x80}, /* Electric Guitar(clean) */
		{0x03, 0x21, 0x47, 0x09, 0xf9, 0xf6, 0x54, 0x3a, 0x00, 0x00, 0x00, 0x80}, /* Electric Guitar(muted) */
		{0x23, 0x21, 0x4a, 0x0e, 0x91, 0x84, 0x41, 0x19, 0x01, 0x00, 0x08, 0x80}, /* Overdriven Guitar */
		{0x23, 0x21, 0x4a, 0x09, 0x95, 0x94, 0x19, 0x19, 0x01, 0x00, 0x08, 0x80}, /* Distortion Guitar */
		{0x09, 0x84, 0xa1, 0x89, 0x20, 0xd1, 0x4f, 0xf8, 0x00, 0x00, 0x08, 0x80}, /* Guitar Harmonics */
		{0x21, 0xa2, 0x1e, 0x09, 0x94, 0xc3, 0x06, 0xa6, 0x00, 0x00, 0x02, 0x80}, /* Acoustic Bass */
		{0x31, 0x31, 0x12, 0x09, 0xf1, 0xf1, 0x28, 0x18, 0x00, 0x00, 0x0a, 0x80}, /* Electric Bass(finger) */
		{0x31, 0x31, 0x8d, 0x09, 0xf1, 0xf1, 0xe8, 0x78, 0x00, 0x00, 0x0a, 0x80}, /* Electric Bass(pick) */
		{0x31, 0x32, 0x5b, 0x09, 0x51, 0x71, 0x28, 0x48, 0x00, 0x00, 0x0c, 0x80}, /* Fretless Bass */
		{0x01, 0x21, 0x8b, 0x49, 0xa1, 0xf2, 0x9a, 0xdf, 0x00, 0x00, 0x08, 0x80}, /* Slap Bass 1 */
		{0x21, 0x21, 0x8b, 0x11, 0xa2, 0xa1, 0x16, 0xdf, 0x00, 0x00, 0x08, 0x80}, /* Slap Bass 2 */
		{0x31, 0x31, 0x8b, 0x09, 0xf4, 0xf1, 0xe8, 0x78, 0x00, 0x00, 0x0a, 0x80}, /* Synth Bass 1 */
		{0x31, 0x31, 0x12, 0x09, 0xf1, 0xf1, 0x28, 0x18, 0x00, 0x00, 0x0a, 0x80}, /* Synth Bass 2 */
		{0x31, 0x21, 0x15, 0x09, 0xdd, 0x56, 0x13, 0x26, 0x01, 0x00, 0x08, 0x80}, /* Violin */
		{0x31, 0x21, 0x16, 0x09, 0xdd, 0x66, 0x13, 0x06, 0x01, 0x00, 0x08, 0x80}, /* Viola */
		{0x71, 0x31, 0x49, 0x09, 0xd1, 0x61, 0x1c, 0x0c, 0x01, 0x00, 0x08, 0x80}, /* Cello */
		{0x21, 0x23, 0x4d, 0x89, 0x71, 0x72, 0x12, 0x06, 0x01, 0x00, 0x02, 0x80}, /* Contrabass */
		{0xf1, 0xe1, 0x40, 0x09, 0xf1, 0x6f, 0x21, 0x16, 0x01, 0x00, 0x02, 0x80}, /* Tremolo Strings */
		{0x02, 0x01, 0x1a, 0x89, 0xf5, 0x85, 0x75, 0x35, 0x01, 0x00, 0x00, 0x80}, /* Pizzicato Strings */
		{0x02, 0x01, 0x1d, 0x89, 0xf5, 0xf3, 0x75, 0xf4, 0x01, 0x00, 0x00, 0x80}, /* Orchestral Strings */
		{0x10, 0x11, 0x41, 0x09, 0xf5, 0xf2, 0x05, 0xc3, 0x01, 0x00, 0x02, 0x80}, /* Timpani */
		{0x21, 0xa2, 0x9b, 0x0a, 0xb1, 0x72, 0x25, 0x08, 0x01, 0x00, 0x0e, 0x80}, /* String Ensemble 1 */
		{0xa1, 0x21, 0x98, 0x09, 0x7f, 0x3f, 0x03, 0x07, 0x01, 0x01, 0x00, 0x80}, /* String Ensemble 2 */
		{0xa1, 0x61, 0x93, 0x09, 0xc1, 0x4f, 0x12, 0x05, 0x00, 0x00, 0x0a, 0x80}, /* SynthStrings 1 */
		{0x21, 0x61, 0x18, 0x09, 0xc1, 0x4f, 0x22, 0x05, 0x00, 0x00, 0x0c, 0x80}, /* SynthStrings 2 */
		{0x31, 0x72, 0x5b, 0x8c, 0xf4, 0x8a, 0x15, 0x05, 0x00, 0x00, 0x00, 0x80}, /* Choir Aahs */
		{0xa1, 0x61, 0x90, 0x09, 0x74, 0x71, 0x39, 0x67, 0x00, 0x00, 0x00, 0x80}, /* Voice Oohs */
		{0x71, 0x72, 0x57, 0x09, 0x54, 0x7a, 0x05, 0x05, 0x00, 0x00, 0x0c, 0x80}, /* Synth Voice */
		{0x90, 0x41, 0x00, 0x09, 0x54, 0xa5, 0x63, 0x45, 0x00, 0x00, 0x08, 0x80}, /* Orchestra Hit */
		{0x21, 0x21, 0x92, 0x0a, 0x85, 0x8f, 0x17, 0x09, 0x00, 0x00, 0x0c, 0x80}, /* Trumpet */
		{0x21, 0x21, 0x94, 0x0e, 0x75, 0x8f, 0x17, 0x09, 0x00, 0x00, 0x0c, 0x80}, /* Trombone */
		{0x21, 0x61, 0x94, 0x09, 0x76, 0x82, 0x15, 0x37, 0x00, 0x00, 0x0c, 0x80}, /* Tuba */
		{0x31, 0x21, 0x43, 0x09, 0x9e, 0x62, 0x17, 0x2c, 0x01, 0x01, 0x02, 0x80}, /* Muted Trumpet */
		{0x21, 0x21, 0x9b, 0x09, 0x61, 0x7f, 0x6a, 0x0a, 0x00, 0x00, 0x02, 0x80}, /* French Horn */
		{0x61, 0x22, 0x8a, 0x0f, 0x75, 0x74, 0x1f, 0x0f, 0x00, 0x00, 0x08, 0x80}, /* Brass Section */
		{0xa1, 0x21, 0x86, 0x8c, 0x72, 0x71, 0x55, 0x18, 0x01, 0x00, 0x00, 0x80}, /* SynthBrass 1 */
		{0x21, 0x21, 0x4d, 0x09, 0x54, 0xa6, 0x3c, 0x1c, 0x00, 0x00, 0x08, 0x80}, /* SynthBrass 2 */
		{0x31, 0x61, 0x8f, 0x09, 0x93, 0x72, 0x02, 0x0b, 0x01, 0x00, 0x08, 0x80}, /* Soprano Sax */
		{0x31, 0x61, 0x8e, 0x09, 0x93, 0x72, 0x03, 0x09, 0x01, 0x00, 0x08, 0x80}, /* Alto Sax */
		{0x31, 0x61, 0x91, 0x09, 0x93, 0x82, 0x03, 0x09, 0x01, 0x00, 0x0a, 0x80}, /* Tenor Sax */
		{0x31, 0x61, 0x8e, 0x09, 0x93, 0x72, 0x0f, 0x0f, 0x01, 0x00, 0x0a, 0x80}, /* Baritone Sax */
		{0x21, 0x21, 0x4b, 0x09, 0xaa, 0x8f, 0x16, 0x0a, 0x01, 0x00, 0x08, 0x80}, /* Oboe */
		{0x31, 0x21, 0x90, 0x09, 0x7e, 0x8b, 0x17, 0x0c, 0x01, 0x01, 0x06, 0x80}, /* English Horn */
		{0x31, 0x32, 0x81, 0x09, 0x75, 0x61, 0x19, 0x19, 0x01, 0x00, 0x00, 0x80}, /* Bassoon */
		{0x32, 0x21, 0x90, 0x09, 0x9b, 0x72, 0x21, 0x17, 0x00, 0x00, 0x04, 0x80}, /* Clarinet */
		{0xe1, 0xe1, 0x1f, 0x09, 0x85, 0x65, 0x5f, 0x1a, 0x00, 0x00, 0x00, 0x80}, /* Piccolo */
		{0xe1, 0xe1, 0x46, 0x09, 0x88, 0x65, 0x5f, 0x1a, 0x00, 0x00, 0x00, 0x80}, /* Flute */
		{0xa1, 0x21, 0x9c, 0x09, 0x75, 0x75, 0x1f, 0x0a, 0x00, 0x00, 0x02, 0x80}, /* Recorder */
		{0x31, 0x21, 0x8b, 0x09, 0x84, 0x65, 0x58, 0x1a, 0x00, 0x00, 0x00, 0x80}, /* Pan Flute */
		{0xe1, 0xa1, 0x4c, 0x09, 0x66, 0x65, 0x56, 0x26, 0x00, 0x00, 0x00, 0x80}, /* Blown Bottle */
		{0x62, 0xa1, 0xcb, 0x09, 0x76, 0x55, 0x46, 0x36, 0x00, 0x00, 0x00, 0x80}, /* Skakuhachi */
		{0x62, 0xa1, 0xa2, 0x09, 0x57, 0x56, 0x07, 0x07, 0x00, 0x00, 0x0b, 0x80}, /* Whistle */
		{0x62, 0xa1, 0x9c, 0x09, 0x77, 0x76, 0x07, 0x07, 0x00, 0x00, 0x0b, 0x80}, /* Ocarina */
		{0x22, 0x21, 0x59, 0x09, 0xff, 0xff, 0x03, 0x0f, 0x02, 0x00, 0x00, 0x80}, /* Lead 1 (square) */
		{0x21, 0x21, 0x0e, 0x09, 0xff, 0xff, 0x0f, 0x0f, 0x01, 0x01, 0x00, 0x80}, /* Lead 2 (sawtooth) */
		{0x22, 0x21, 0x46, 0x89, 0x86, 0x64, 0x55, 0x18, 0x00, 0x00, 0x00, 0x80}, /* Lead 3 (calliope) */
		{0x21, 0xa1, 0x45, 0x09, 0x66, 0x96, 0x12, 0x0a, 0x00, 0x00, 0x00, 0x80}, /* Lead 4 (chiff) */
		{0x21, 0x22, 0x8b, 0x09, 0x92, 0x91, 0x2a, 0x2a, 0x01, 0x00, 0x00, 0x80}, /* Lead 5 (charang) */
		{0xa2, 0x61, 0x9e, 0x49, 0xdf, 0x6f, 0x05, 0x07, 0x00, 0x00, 0x02, 0x80}, /* Lead 6 (voice) */
		{0x20, 0x60, 0x1a, 0x09, 0xef, 0x8f, 0x01, 0x06, 0x00, 0x02, 0x00, 0x80}, /* Lead 7 (fifths) */
		{0x21, 0x21, 0x8f, 0x86, 0xf1, 0xf4, 0x29, 0x09, 0x00, 0x00, 0x0a, 0x80}, /* Lead 8 (bass+lead) */
		{0x77, 0xa1, 0xa5, 0x09, 0x53, 0xa0, 0x94, 0x05, 0x00, 0x00, 0x02, 0x80}, /* Pad 1 (new age) */
		{0x61, 0xb1, 0x1f, 0x89, 0xa8, 0x25, 0x11, 0x03, 0x00, 0x00, 0x0a, 0x80}, /* Pad 2 (warm) */
		{0x61, 0x61, 0x17, 0x09, 0x91, 0x55, 0x34, 0x16, 0x00, 0x00, 0x0c, 0x80}, /* Pad 3 (polysynth) */
		{0x71, 0x72, 0x5d, 0x09, 0x54, 0x6a, 0x01, 0x03, 0x00, 0x00, 0x00, 0x80}, /* Pad 4 (choir) */
		{0x21, 0xa2, 0x97, 0x09, 0x21, 0x42, 0x43, 0x35, 0x00, 0x00, 0x08, 0x80}, /* Pad 5 (bowed) */
		{0xa1, 0x21, 0x1c, 0x09, 0xa1, 0x31, 0x77, 0x47, 0x01, 0x01, 0x00, 0x80}, /* Pad 6 (metallic) */
		{0x21, 0x61, 0x89, 0x0c, 0x11, 0x42, 0x33, 0x25, 0x00, 0x00, 0x0a, 0x80}, /* Pad 7 (halo) */
		{0xa1, 0x21, 0x15, 0x09, 0x11, 0xcf, 0x47, 0x07, 0x01, 0x00, 0x00, 0x80}, /* Pad 8 (sweep) */
		{0x3a, 0x51, 0xce, 0x09, 0xf8, 0x86, 0xf6, 0x02, 0x00, 0x00, 0x02, 0x80}, /* FX 1 (rain) */
		{0x21, 0x21, 0x15, 0x09, 0x21, 0x41, 0x23, 0x13, 0x01, 0x00, 0x00, 0x80}, /* FX 2 (soundtrack) */
		{0x06, 0x01, 0x5b, 0x09, 0x74, 0xa5, 0x95, 0x72, 0x00, 0x00, 0x00, 0x80}, /* FX 3 (crystal) */
		{0x22, 0x61, 0x92, 0x8c, 0xb1, 0xf2, 0x81, 0x26, 0x00, 0x00, 0x0c, 0x80}, /* FX 4 (atmosphere) */
		{0x41, 0x42, 0x4d, 0x09, 0xf1, 0xf2, 0x51, 0xf5, 0x01, 0x00, 0x00, 0x80}, /* FX 5 (brightness) */
		{0x61, 0xa3, 0x94, 0x89, 0x11, 0x11, 0x51, 0x13, 0x01, 0x00, 0x06, 0x80}, /* FX 6 (goblins) */
		{0x61, 0xa1, 0x8c, 0x89, 0x11, 0x1d, 0x31, 0x03, 0x00, 0x00, 0x06, 0x80}, /* FX 7 (echoes) */
		{0xa4, 0x61, 0x4c, 0x09, 0xf3, 0x81, 0x73, 0x23, 0x01, 0x00, 0x04, 0x80}, /* FX 8 (sci-fi) */
		{0x02, 0x07, 0x85, 0x0c, 0xd2, 0xf2, 0x53, 0xf6, 0x00, 0x01, 0x00, 0x80}, /* Sitar */
		{0x11, 0x13, 0x0c, 0x89, 0xa3, 0xa2, 0x11, 0xe5, 0x01, 0x00, 0x00, 0x80}, /* Banjo */
		{0x11, 0x11, 0x06, 0x09, 0xf6, 0xf2, 0x41, 0xe6, 0x01, 0x02, 0x04, 0x80}, /* Shamisen */
		{0x93, 0x91, 0x91, 0x09, 0xd4, 0xeb, 0x32, 0x11, 0x00, 0x01, 0x08, 0x80}, /* Koto */
		{0x04, 0x01, 0x4f, 0x09, 0xfa, 0xc2, 0x56, 0x05, 0x00, 0x00, 0x0c, 0x80}, /* Kalimba */
		{0x21, 0x22, 0x49, 0x09, 0x7c, 0x6f, 0x20, 0x0c, 0x00, 0x01, 0x06, 0x80}, /* Bagpipe */
		{0x31, 0x21, 0x85, 0x09, 0xdd, 0x56, 0x33, 0x16, 0x01, 0x00, 0x0a, 0x80}, /* Fiddle */
		{0x20, 0x21, 0x04, 0x8a, 0xda, 0x8f, 0x05, 0x0b, 0x02, 0x00, 0x06, 0x80}, /* Shanai */
		{0x05, 0x03, 0x6a, 0x89, 0xf1, 0xc3, 0xe5, 0xe5, 0x00, 0x00, 0x06, 0x80}, /* Tinkle Bell */
		{0x07, 0x02, 0x15, 0x09, 0xec, 0xf8, 0x26, 0x16, 0x00, 0x00, 0x0a, 0x80}, /* Agogo */
		{0x05, 0x01, 0x9d, 0x09, 0x67, 0xdf, 0x35, 0x05, 0x00, 0x00, 0x08, 0x80}, /* Steel Drums */
		{0x18, 0x12, 0x96, 0x09, 0xfa, 0xf8, 0x28, 0xe5, 0x00, 0x00, 0x0a, 0x80}, /* Woodblock */
		{0x10, 0x00, 0x86, 0x0c, 0xa8, 0xfa, 0x07, 0x03, 0x00, 0x00, 0x06, 0x80}, /* Taiko Drum */
		{0x11, 0x10, 0x41, 0x0c, 0xf8, 0xf3, 0x47, 0x03, 0x02, 0x00, 0x04, 0x80}, /* Melodic Tom */
		{0x01, 0x10, 0x8e, 0x09, 0xf1, 0xf3, 0x06, 0x02, 0x02, 0x00, 0x0e, 0x80}, /* Synth Drum */
		{0x0e, 0xc0, 0x00, 0x09, 0x1f, 0x1f, 0x00, 0xff, 0x00, 0x03, 0x0e, 0x80}, /* Reverse Cymbal */
		{0x06, 0x03, 0x80, 0x91, 0xf8, 0x56, 0x24, 0x84, 0x00, 0x02, 0x0e, 0x80}, /* Guitar Fret Noise */
		{0x0e, 0xd0, 0x00, 0x0e, 0xf8, 0x34, 0x00, 0x04, 0x00, 0x03, 0x0e, 0x80}, /* Breath Noise */
		{0x0e, 0xc0, 0x00, 0x09, 0xf6, 0x1f, 0x00, 0x02, 0x00, 0x03, 0x0e, 0x80}, /* Seashore */
		{0xd5, 0xda, 0x95, 0x49, 0x37, 0x56, 0xa3, 0x37, 0x00, 0x00, 0x00, 0x80}, /* Bird Tweet */
		{0x35, 0x14, 0x5c, 0x11, 0xb2, 0xf4, 0x61, 0x15, 0x02, 0x00, 0x0a, 0x80}, /* Telephone ring */
		{0x0e, 0xd0, 0x00, 0x09, 0xf6, 0x4f, 0x00, 0xf5, 0x00, 0x03, 0x0e, 0x80}, /* Helicopter */
		{0x26, 0xe4, 0x00, 0x09, 0xff, 0x12, 0x01, 0x16, 0x00, 0x01, 0x0e, 0x80}, /* Applause */
		{0x00, 0x00, 0x00, 0x09, 0xf3, 0xf6, 0xf0, 0xc9, 0x00, 0x02, 0x0e, 0x80}  /* Gunshot */
};

/* logarithmic relationship between midi and FM volumes */
const int Opl3StereoMidiDriver::my_midi_fm_vol_table[128]
		= {0,   11,  16,  19,  22,  25,  27,  29,  32,  33,  35,  37,  39,  40,  42,  43,  45,  46,  48,  49,  50,  51,
		   53,  54,  55,  56,  57,  58,  59,  60,  61,  62,  64,  64,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,
		   75,  75,  76,  77,  78,  79,  80,  80,  81,  82,  83,  83,  84,  85,  86,  86,  87,  88,  89,  89,  90,  91,
		   91,  92,  93,  93,  94,  95,  96,  96,  97,  97,  98,  99,  99,  100, 101, 101, 102, 103, 103, 104, 104, 105,
		   106, 106, 107, 107, 108, 109, 109, 110, 110, 111, 112, 112, 113, 113, 114, 114, 115, 115, 116, 117, 117, 118,
		   118, 119, 119, 120, 120, 121, 121, 122, 122, 123, 123, 124, 124, 125, 125, 126, 126, 127};

/* lucas move volume table */
int Opl3StereoMidiDriver::lucas_fm_vol_table[128];

const unsigned short Opl3StereoMidiDriver::adlib_opadd[VoiceCount] = {
	0x00, 0x01, 0x02, 0x08, 0x09, 0x0A, 0x10, 0x11, 0x12,
	0x100, 0x101, 0x102, 0x108, 0x109, 0x10A, 0x110, 0x111, 0x112};

//
// Open the device
//
int Opl3StereoMidiDriver::open() {
	for (int i = 0; i < 128; i++) {
		myinsbank[i] = midi_fm_instruments_table[i];

		// Setup square root table here
		lucas_fm_vol_table[i] = static_cast<int>(std::sqrt(static_cast<double>(my_midi_fm_vol_table[i])) * 11); /* TO CHANGE !!! */

		// Clear the xmidibanks
		xmidibanks[i] = nullptr;
	}

	for (auto& i : ch) {
		i.inum       = 0;
		i.ins        = myinsbank[0];
		i.xmidi      = false;
		i.xmidi_bank = 0;
		i.vol        = 127;
		i.expression = 127;
		i.nshift     = -13;    //-13;
		i.enabled    = 1;
		i.pitchbend  = 0x2000;
		i.pan        = 64;
		i.sustain    = 0;
		i.modulation = 0;
	}

	/* General init */
	for (auto& i : chp) {
		i = {-1, 0, 0, 0, false};
	}

	bg_bank = false;
	si_bank = false;
	bg_tick_phase = 0;
	std::fill_n(timbre_valid, VoiceCount, false);
	for (auto& voice : bg_voice) voice = {};
	std::fill_n(adlib_data, 512, 0);
	std::fill_n(voice_identity, VoiceCount, 0);
	note_identity = 0;
	std::fill_n(release_order, VoiceCount, 0);
	release_clock = 0;
	std::fill_n(voice_pan,VoiceCount,64);
	opl = NukedStereo::makeAdLibOPL(sample_rate);
	if (!opl) return 1;

	return 0;
}

//
// Close the device
//
void Opl3StereoMidiDriver::close() {
	ending_voices={}; ending_patches={}; ending_context=ending_bank=false; original_menu=false; ending_tick_phase=0;
	// Destroy the Opl device
	if (opl) {
		NukedStereo::OPLDestroy(opl);
	}

	// Reset the relevant members
	opl = nullptr;
	for (auto& voice : chp) voice = {-1, 0, 0, 0, false};
	std::fill_n(voice_identity, VoiceCount, 0);
	note_identity = 0;

	// Clear the xmidibanks
	for (auto*& xmidibank : xmidibanks) {
		delete xmidibank;
		xmidibank = nullptr;
	}
}

//
// Generate the samples
//

void Opl3StereoMidiDriver::lowLevelProduceSamples(sint16* samples, uint32 num_samples) {
	if (!opl) { std::fill_n(samples,num_samples*(stereo?2:1),0); return; }
	while (num_samples) {
		const uint32 frames=(bg_bank || ending_bank)?std::min<uint32>(num_samples,(sample_rate-(ending_bank?ending_tick_phase:bg_tick_phase)+59)/60):num_samples;
		if (stereo) NukedStereo::YM3812UpdateOne_Stereo(opl,samples,frames);
		else NukedStereo::YM3812UpdateOne_Mono(opl,samples,frames);
		if (bg_bank || si_bank) {
			const int gain=LowLevelMidiDriver::getGlobalVolume();
			for (uint32 i=0;i<frames*(stereo?2:1);++i) samples[i]=sint16(int(samples[i])*gain/100);
		}
		samples+=frames*(stereo?2:1); num_samples-=frames;
		if (ending_bank) { ending_tick_phase+=uint64(frames)*60; if(ending_tick_phase>=sample_rate) {ending_tick_phase-=sample_rate;tickEndingVoices();} }
		if (bg_bank) { bg_tick_phase+=uint64(frames)*60; if(bg_tick_phase>=sample_rate) {bg_tick_phase-=sample_rate;tickBgVoices();} }
	}
}

int Opl3StereoMidiDriver::midi_calc_volume(int channel, int vel) {
	vel += (VEL_FUDGE - 1) * 128;

	int nv = (ch[channel].vol * ch[channel].expression * vel) / (16129 * VEL_FUDGE);
	//	int nv = (ch[channel].vol * ch[channel].expression * vel) >> 14;

	if (nv > 127) {
		nv = 127;
	}

	nv = my_midi_fm_vol_table[nv];

	return nv;
}

//
// Send a single packed midi command to the Opl (to be played now)
//
void Opl3StereoMidiDriver::sendNote(uint32 message, uint64 identity) {
	note_identity = identity;
	send(message);
	note_identity = 0;
}

void Opl3StereoMidiDriver::send(uint32 b) {
	const auto channel = uint8(b & 0x0F);

	// BG classic introduction/menu triggers use original mapping, not an XMIDI drum bank.
	if (channel == 9 && !xmidibanks[127] && !original_intro && !original_menu) {
		return;
	}

	switch (b & MidiStatus::TypeMask) {
	case MidiStatus::NoteOff: /*note off */
		// Quick hack, but we should never use it. since note offs will never be
		// sent
		b &= 0xFFFF;
		[[fallthrough]];
	case MidiStatus::NoteOn: { /*note on */
		auto note = static_cast<unsigned char>((b >> 8) & 0x7F);
		auto vel  = static_cast<unsigned char>((b >> 16) & 0x7F);
		int introPart=-1;
		if((original_intro || original_menu) && channel==9) {
			if(note>=intro_channels.size() || intro_channels[note]<10 || intro_channels[note]>30) return;
			introPart=intro_channels[note]-10;
			note=intro_keys[note];
		}


		// First send a note off, if it's found
		for (int i = 0; i < VoiceCount; i++) {
			if ((!note_identity || vel == 0) && (chp[i].channel == channel) && (chp[i].note == note)
				&& (!note_identity || voice_identity[i] == note_identity)) {
				if (ownsEndingVoice(i)) {
					if(ch[channel].sustain>=64) chp[i].sustained=true;
					else releaseEndingVoice(i);
				} else if (bg_bank || ch[channel].sustain < 64) {
					midi_fm_endnote(i);
					chp[i].channel = -1;
				} else {
					chp[i].sustained = true;
					chp[i].counter += 10;    // artificially age the chp
				}
			}
		}

		if (vel != 0 && ch[channel].enabled != 0) {
			auto startPartial=[&](xinstrument instrument,bool secondary) {
			// Increment each counter
			for (auto& i : chp) {
				i.counter++;
			}

			// Prefer the voice whose release envelope has had longest to decay.
			int j   = 0;
			int onl = 0;
			int on  = -1;
			for (int i = 0; i < VoiceCount; i++) {
				if (chp[i].channel == -1 && (on == -1 || release_order[i] < release_order[on])) {
					on  = i;
					j   = 1;
				}
			}

			// At capacity, retain the existing oldest-active-note stealing policy.
			// endnote invalidates its identity before the replacement is programmed.
			if (on == -1 && secondary) return;
			if (on == -1) {
				onl = 0;
				for (int i = 0; i < VoiceCount; i++) {
					if (chp[i].counter > onl) {
						onl = chp[i].counter;
						on  = i;
					}
				}
			}

			// If we didn't find a free note, send a note off to the Adlib for
			// the chan we are using
			if (j == 0) {
				midi_fm_endnote(on);
			}

			if(ending_bank && instrument.tvfx_program>=0) {
				chp[on]={channel,note,0,vel,false};
				voice_identity[on]=note_identity;
				voice_pan[on]=ch[channel].pan;
				startEndingVoice(on,instrument.tvfx_program,channel,note,vel);
				return;
			}
			midi_fm_instrument(on,instrument);

			// Calculate the adlib volume
			const int nv = midi_calc_volume(channel, vel);

			// Install the occurrence before calculating its timbre-specific level.
			chp[on] = {channel, note, 0, vel, false};
			voice_identity[on] = note_identity;
			voice_pan[on]=ch[channel].pan;
			bg_voice[on]={};
			if (voice_timbre[on].native_bg) {
				bg_voice[on].channel=channel; bg_voice[on].intro_part=introPart; bg_voice[on].keyed=true; bg_voice[on].stage=0;
				bg_voice[on].pitch=int(note)*256;
				bg_voice[on].slide=sint16(voice_timbre[on].bg[18]|(voice_timbre[on].bg[19]<<8));
			}
			applyModulation(on);
			if (voice_timbre[on].native_bg) midi_fm_volume(on,nv*2);
			midi_fm_playnote(on, note + ch[channel].nshift, nv * 2, ch[channel].pitchbend, ch[channel].pan);
			};
			xinstrument instrument=introPart>=0 ? intro_parts[introPart] : channel==9 && xmidibanks[127]?xmidibanks[127]->insbank[note]:ch[channel].ins;
			std::array<bool,256> visited{};
			bool secondary=false;
			for (;;) {
				instrument.perc_voice &= 0x80;
				startPartial(instrument,secondary);
				if(!instrument.native_bg || !instrument.bg[38]) break;
				unsigned next=instrument.bg[38]-1;
				if(visited[next] || !(bg_timbres[next].perc_voice&0x80)) break;
				visited[next]=true;instrument=bg_timbres[next];secondary=true;
			}

		}
	} break;

	case MidiStatus::Aftertouch: { /*key after touch */
		if (bg_bank) break;
		auto      note = static_cast<unsigned char>((b >> 8) & 0x7F);
		auto      vel  = static_cast<unsigned char>((b >> 16) & 0x7F);
		const int nv   = midi_calc_volume(channel, vel);

		for (int i = 0; i < VoiceCount; i++) {
			if ((chp[i].channel == channel) && (chp[i].note == note)) {
				chp[i].velocity = vel;
				midi_fm_volume(i, nv * 2);
			}
		}
	} break;

	case MidiStatus::Controller: { /* control change */
		auto ctrl = MidiController((b >> 8) & 0x7F);
		auto vel  = uint8((b >> 16) & 0x7F);

		// BG DOS controller behavior differs from SI's AIL FM driver.
		switch (ctrl) {
		case MidiController::Bank: /* Bank Change */
			break;
		case MidiController::ModWheel:
			ch[channel].modulation=vel;
			for(int i=0;i<VoiceCount;++i) if(chp[i].channel==channel) applyModulation(i);
			break;
		case MidiController::Volume: /* Volume */
			ch[channel].vol = vel;
			if (!bg_bank) midi_update_volume(channel);
			break;
		case MidiController::Balance: // MIDI CC10; capture on the next note only.
			ch[channel].pan = vel;
			break;
		case MidiController::Expression: /* Expression */
			ch[channel].expression = vel;
			if (!bg_bank) midi_update_volume(channel);
			break;
		case MidiController::Sustain: /* BG DOS FM ignores CC64; SI supports it. */
			if (bg_bank) break;
			ch[channel].sustain = vel;

			// if sustain ending turn off voices as needed
			if (vel < 64) {
				for (int i = 0; i < VoiceCount; i++) {
					if (chp[i].channel == channel && chp[i].sustained) {
						if(ownsEndingVoice(i)) {chp[i].sustained=false;releaseEndingVoice(i);}
						else midi_fm_endnote(i);
					}
				}
			}
			break;
		case MidiController::ResetControllers: /* Reset all controllers*/
			// we'll just call ourself with all the controllers'
			// default values
			send(channel | (int(MidiStatus::PitchWheel)) | 0x400000);
			send(channel | (int(MidiStatus::Controller)) | (int(MidiController::Bank) << 8) | 0);
			send(channel | (int(MidiStatus::Controller)) | (int(MidiController::ModWheel) << 8));
			send(channel | (int(MidiStatus::Controller)) | (int(MidiController::Volume) << 8) | 0x7F0000);
			send(channel | (int(MidiStatus::Controller)) | (int(MidiController::Expression) << 8) | 0x7F0000);
			send(channel | (int(MidiStatus::Controller)) | (int(MidiController::Pan) << 8) | 0x400000);
			send(channel | (int(MidiStatus::Controller)) | (int(MidiController::Sustain) << 8) | 0);
			send(channel | (int(MidiStatus::Controller)) | (int(MidiController::Effect) << 8) | 0);
			send(channel | (int(MidiStatus::Controller)) | (int(MidiController::Chorus) << 8) | 0);
			send(channel | (int(MidiStatus::Controller)) | (int(MidiController::XBankChange) << 8) | 0);
			break;
		case MidiController::Effect: /* Extended depth effect */
			// debug(1,
			//			"MIDI sub-command 0xB0 (Control Change) case %02X
			//(Extended Depth) not handled in MIDIEMU driver.", ctrl);
			break;
		case MidiController::Chorus: /* Chorus depth */
			// debug(1,
			//			"MIDI sub-command 0xB0 (Control Change) case %02X
			//(Chorus Depth) not handled in MIDIEMU driver.", 			ctrl);
			break;
		//
		// XMIDI
		//
		case MidiController::XChanLock:
			// PERR("Uh oh. Detected a XMIDI Channel Lock (" << (int) vel << ")
			// on channel " << (int) channel << ", but we don't support them");
			break;
		case MidiController::XChanLockProtect:
			// PERR("Uh oh. Detected a XMIDI Channel Lock Protect (" << (int)
			// vel << ") on channel " << (int) channel << ", but we don't
			// support them");
			break;
		case MidiController::XVoiceProtect:
			// PERR("Uh oh. Detected a XMIDI Voice Protect (" << (int) vel << ")
			// on channel " << (int) channel << ", but we don't support them");
			break;
		case MidiController::XTimbreProtect:
			// PERR("Uh oh. Detected a XMIDI Timbre Protect (" << (int) vel <<
			// ") on channel " << (int) channel << ", but we don't support
			// them");
			break;
		case MidiController::XBankChange:
			// POUT("Detected a XMIDI Bank Change (" << (int) vel << ") on
			// channel " << (int) channel);
			//  Set the bank
			ch[channel].xmidi_bank = vel;
			break;
		case MidiController::XIndirectCtrlPrefix:
			// PERR("Uh oh. Detected a XMIDI Indirect Controller Prefix (" <<
			// (int) vel << ") on channel " << (int) channel << ", but we don't
			// support them");
			break;
		case MidiController::XClearBBCount:
			// PERR("Uh oh. Detected a XMIDI Clear Beat/Bar Count (" << (int)
			// vel << ") on channel " << (int) channel << ", but we don't
			// support them");
			break;
		case MidiController::XCallbackTrigger:
			// PERR("Uh oh. Detected a XMIDI Callback Trigger (" << (int) vel <<
			// ") on channel " << (int) channel << ", but we don't support
			// them");
			break;
		case MidiController::XSequenceBranchIndex:
			// PERR("Uh oh. Detected a XMIDI Sequence Branch Index (" << (int)
			// vel << ") on channel " << (int) channel << ", but we don't
			// support them");
			break;
		//
		// END XMIDI
		//
		case MidiController::AllNotesOff: /* All notes off */
			for (int i = 0; i < VoiceCount; i++) {
				if (chp[i].channel == channel) {
					midi_fm_endnote(i);
				}
			}
			break;
		default:
			// PERR("MIDI sub-command 0xB0 (Control Change) case 0x" << std::hex
			// << (int) ctrl << std::dec << " not handled in MIDIEMU driver.");
			break;
		}
	} break;

	case MidiStatus::Program: { /* patch change */
		if((original_intro || original_menu) && channel==9) break;
		auto instrument  = static_cast<unsigned char>((b >> 8) & 0x7F);
		ch[channel].inum = instrument;
		// std::POUT << "Setting instrument: " << static_cast<unsigned
		// int>(instrument) << " for chan " << static_cast<unsigned
		// int>(channel) << std::endl;

		xinstrument* ins = nullptr;

		// Search for xmidi ins.
		if (ch[channel].xmidi) {
			for (int b = ch[channel].xmidi_bank; b >= 0; b--) {
				xmidibank* bank = xmidibanks[b];
				if (!bank) {
					continue;
				}
				if (bank->insbank[instrument].perc_voice & 0x80) {
					ins = &bank->insbank[instrument];
					break;
				}
			}
		}

		if (!ins) {
			ins = bg_bank && bg_default.native_bg ? &bg_default : &myinsbank[instrument];
		}

		if (bg_bank) for(int i=0;i<VoiceCount;++i) if(chp[i].channel==channel) midi_fm_endnote(i);
		ch[channel].ins = *ins;
		// The accepted backend renders two-operator melodic voices, not hardware rhythm slots.
		ch[channel].ins.perc_voice &= 0x80;

		// POUT("Detected a Patch Change (" << (int) instrument << ") on channel
		// " << (int) channel << " xmidibank: " << b);

	} break;

	case MidiStatus::ChannelTouch: /* channel touch */
		// debug(1, "MIDI command 0xD0 (Channel Touch) not handled in MIDIEMU
		// driver.");
		break;

	case MidiStatus::PitchWheel: {
		// break;						/* pitch wheel */
		const int pitchbend   = ((b >> 8) & 0x7F) | (((b >> 16) & 0x7F) << 7);
		ch[channel].pitchbend = pitchbend;
		for (int i = 0; i < VoiceCount; i++) {
			if (chp[i].channel == channel) {
				const int nv = midi_calc_volume(channel, chp[i].velocity);
				midi_fm_playnote(i, chp[i].note + ch[channel].nshift, nv * 2, pitchbend, ch[channel].pan);
			}
		}
	} break;

	default:
		// fprintf(stderr, "Unknown : %08x\n", (int)b);
		break;
	}
}

void Opl3StereoMidiDriver::midi_write_adlib(unsigned int reg, unsigned char val) {
	const unsigned port = (reg & 0x100) ? 2 : 0;
	NukedStereo::OPLWrite(opl, port, reg & 0xff);
	NukedStereo::OPLWrite(opl, port | 1, val);
	adlib_data[reg] = val;
}

/*

struct AD_instrument
{
   unsigned char mod_avekm;		// 0	(20)
   unsigned char mod_ksl_tl;	// 1	(40)
   unsigned char mod_ad;		// 2	(60)
   unsigned char mod_sr;		// 3	(80)
   unsigned char mod_ws;		// 4	(E0)

   unsigned char fb_c;			// 5

   unsigned char car_avekm;		// 6	amp, sussnd				(20)
   unsigned char car_ksl_tl;	// 7	outlev, keyscale		(43)
   unsigned char car_ad;		// 8	Attack Delay		AR	(63)
   unsigned char car_sr;		// 9	SustainLev Release	DR	(83)
   unsigned char car_ws;		// 10	Waveform				(E0)
};

case 0x20:	 am,vib,ksr,eg type,mul

*/

void Opl3StereoMidiDriver::midi_fm_instrument(int voice, xinstrument& inst) {
	if(timbre_valid[voice] && voice_timbre[voice]==inst) return;
	voice_timbre[voice]=inst; timbre_valid[voice]=true;
	midi_write_adlib(0x20 + adlib_opadd[voice], inst.mod_avekm);
	midi_write_adlib(0x40 + adlib_opadd[voice], inst.mod_ksl_tl);
	midi_write_adlib(0x60 + adlib_opadd[voice], inst.mod_ad);
	midi_write_adlib(0x80 + adlib_opadd[voice], inst.mod_sr);
	midi_write_adlib(0xe0 + adlib_opadd[voice], inst.mod_ws);

	if (!(inst.perc_voice & 0x7f)) {
		midi_write_adlib(0x23 + adlib_opadd[voice], inst.car_avekm);
		midi_write_adlib(0x43 + adlib_opadd[voice], inst.car_ksl_tl);
		midi_write_adlib(0x63 + adlib_opadd[voice], inst.car_ad);
		midi_write_adlib(0x83 + adlib_opadd[voice], inst.car_sr);
		midi_write_adlib(0xe3 + adlib_opadd[voice], inst.car_ws);
		midi_write_adlib(0xc0 + channelRegister(voice), inst.fb_c);
		midi_write_adlib(0xbd, 0);

		if (!inst.perc_voice) {
			PERR("Tried setting an instruement that wasnt loaded");
		}
	}
}

void Opl3StereoMidiDriver::midi_update_volume(int channel) {
	for (int i = 0; i < VoiceCount; i++) {
		if (chp[i].channel == channel) {
			const int nv = midi_calc_volume(channel, chp[i].velocity);
			midi_fm_volume(i, nv * 2);
		}
	}
}

void Opl3StereoMidiDriver::midi_fm_volume(int voice, int volume) {
	if(ownsEndingVoice(voice)) { ending_voices[voice].update|=0x40; updateEndingVoice(voice); return; }
	const auto& instrument=voice_timbre[voice];
	if (instrument.native_si && chp[voice].channel>=0) {
		const auto& channel=ch[chp[voice].channel];
		auto scale=[](int x,int y) {int result=x*y/128;return result?result+1:0;};
		const int velocity=82+3*(chp[voice].velocity>>3);
		const int gain=scale(scale(channel.vol,channel.expression),velocity);
		auto level=[&](uint8 base,bool scaled) {
			int amplitude=63-(base&63);
			if(scaled) amplitude=amplitude*gain/127;
			return uint8((base&0xc0)|(63-amplitude));
		};
		midi_write_adlib(0x40+adlib_opadd[voice],level(instrument.mod_ksl_tl,(instrument.fb_c&1)!=0));
		midi_write_adlib(0x43+adlib_opadd[voice],level(instrument.car_ksl_tl,true));
		return;
	}
	if(instrument.native_bg && chp[voice].channel>=0) {
		const int velocity=chp[voice].velocity;
		const int gain=ch[chp[voice].channel].vol+128;
		auto shift=[](int value,unsigned bits) {bits&=31; if(bits>15)return value<0?-1:0; const int divisor=1<<bits; return value<0?-((-value+divisor-1)/divisor):value/divisor;};
		auto level=[&](uint8 base,uint8 sensitivity,bool carrier) {
			int attenuation=(base&63)+shift(63-velocity,uint8(7-sensitivity));
			if(carrier) attenuation=63-shift(sint16(gain*(63-attenuation)),8);
			else if(instrument.fb_c&1) {int scaled=shift(sint16(gain*(63-attenuation)),8);attenuation=scaled>63?63:63-scaled;}
			return uint8((base&0xc0)|std::clamp(attenuation,0,63));
		};
		midi_write_adlib(0x43+adlib_opadd[voice],level(instrument.car_ksl_tl,instrument.bg[12],true));
		midi_write_adlib(0x40+adlib_opadd[voice],level(instrument.mod_ksl_tl,instrument.bg[13],false));
		return;
	}

	volume >>= 2;

	if ((adlib_data[0xc0 + channelRegister(voice)] & 1) == 1) {
		midi_write_adlib(
				0x40 + adlib_opadd[voice],
				static_cast<unsigned char>((63 - volume) | (adlib_data[0x40 + adlib_opadd[voice]] & 0xc0)));
	}
	midi_write_adlib(
			0x43 + adlib_opadd[voice], static_cast<unsigned char>((63 - volume) | (adlib_data[0x43 + adlib_opadd[voice]] & 0xc0)));
}

const int Opl3StereoMidiDriver::fnums[12] = {0x16b, 0x181, 0x198, 0x1b0, 0x1ca, 0x1e5, 0x202, 0x220, 0x241, 0x263, 0x287, 0x2ae};

/* These tables 'borrowed' from Timidity tables.c

   Copyright (C) 1999-2001 Masanao Izumo <mo@goice.co.jp>
   Copyright (C) 1995 Tuukka Toivonen <tt@cgs.fi>
*/
const double Opl3StereoMidiDriver::bend_fine[256]
		= {1.0,
		   1.0002256593050698,
		   1.0004513695322617,
		   1.0006771306930664,
		   1.0009029427989777,
		   1.0011288058614922,
		   1.0013547198921082,
		   1.0015806849023274,
		   1.0018067009036538,
		   1.002032767907594,
		   1.0022588859256572,
		   1.0024850549693551,
		   1.0027112750502025,
		   1.0029375461797159,
		   1.0031638683694153,
		   1.0033902416308227,
		   1.0036166659754628,
		   1.0038431414148634,
		   1.0040696679605541,
		   1.0042962456240678,
		   1.0045228744169397,
		   1.0047495543507072,
		   1.0049762854369111,
		   1.0052030676870944,
		   1.0054299011128027,
		   1.0056567857255843,
		   1.00588372153699,
		   1.006110708558573,
		   1.0063377468018897,
		   1.0065648362784985,
		   1.0067919769999607,
		   1.0070191689778405,
		   1.0072464122237039,
		   1.0074737067491204,
		   1.0077010525656616,
		   1.0079284496849015,
		   1.0081558981184175,
		   1.008383397877789,
		   1.008610948974598,
		   1.0088385514204294,
		   1.0090662052268706,
		   1.0092939104055114,
		   1.0095216669679448,
		   1.0097494749257656,
		   1.009977334290572,
		   1.0102052450739643,
		   1.0104332072875455,
		   1.0106612209429215,
		   1.0108892860517005,
		   1.0111174026254934,
		   1.0113455706759138,
		   1.0115737902145781,
		   1.0118020612531047,
		   1.0120303838031153,
		   1.0122587578762337,
		   1.012487183484087,
		   1.0127156606383041,
		   1.0129441893505169,
		   1.0131727696323602,
		   1.0134014014954713,
		   1.0136300849514894,
		   1.0138588200120575,
		   1.0140876066888203,
		   1.0143164449934257,
		   1.0145453349375237,
		   1.0147742765327674,
		   1.0150032697908125,
		   1.0152323147233171,
		   1.015461411341942,
		   1.0156905596583505,
		   1.0159197596842091,
		   1.0161490114311862,
		   1.0163783149109531,
		   1.0166076701351838,
		   1.0168370771155553,
		   1.0170665358637463,
		   1.0172960463914391,
		   1.0175256087103179,
		   1.0177552228320703,
		   1.0179848887683858,
		   1.0182146065309567,
		   1.0184443761314785,
		   1.0186741975816487,
		   1.0189040708931674,
		   1.0191339960777379,
		   1.0193639731470658,
		   1.0195940021128593,
		   1.0198240829868295,
		   1.0200542157806898,
		   1.0202844005061564,
		   1.0205146371749483,
		   1.0207449257987866,
		   1.0209752663893958,
		   1.0212056589585028,
		   1.0214361035178368,
		   1.0216666000791297,
		   1.0218971486541166,
		   1.0221277492545349,
		   1.0223584018921241,
		   1.0225891065786274,
		   1.0228198633257899,
		   1.0230506721453596,
		   1.023281533049087,
		   1.0235124460487257,
		   1.0237434111560313,
		   1.0239744283827625,
		   1.0242054977406807,
		   1.0244366192415495,
		   1.0246677928971357,
		   1.0248990187192082,
		   1.025130296719539,
		   1.0253616269099028,
		   1.0255930093020766,
		   1.0258244439078401,
		   1.0260559307389761,
		   1.0262874698072693,
		   1.0265190611245079,
		   1.0267507047024822,
		   1.0269824005529853,
		   1.027214148687813,
		   1.0274459491187637,
		   1.0276778018576387,
		   1.0279097069162415,
		   1.0281416643063788,
		   1.0283736740398595,
		   1.0286057361284953,
		   1.0288378505841009,
		   1.0290700174184932,
		   1.0293022366434921,
		   1.0295345082709197,
		   1.0297668323126017,
		   1.0299992087803651,
		   1.030231637686041,
		   1.0304641190414621,
		   1.0306966528584645,
		   1.0309292391488862,
		   1.0311618779245688,
		   1.0313945691973556,
		   1.0316273129790936,
		   1.0318601092816313,
		   1.0320929581168212,
		   1.0323258594965172,
		   1.0325588134325767,
		   1.0327918199368598,
		   1.0330248790212284,
		   1.0332579906975481,
		   1.0334911549776868,
		   1.033724371873515,
		   1.0339576413969056,
		   1.0341909635597348,
		   1.0344243383738811,
		   1.0346577658512259,
		   1.034891246003653,
		   1.0351247788430489,
		   1.0353583643813031,
		   1.0355920026303078,
		   1.0358256936019572,
		   1.0360594373081489,
		   1.0362932337607829,
		   1.0365270829717617,
		   1.0367609849529913,
		   1.0369949397163791,
		   1.0372289472738365,
		   1.0374630076372766,
		   1.0376971208186156,
		   1.0379312868297725,
		   1.0381655056826686,
		   1.0383997773892284,
		   1.0386341019613787,
		   1.0388684794110492,
		   1.0391029097501721,
		   1.0393373929906822,
		   1.0395719291445176,
		   1.0398065182236185,
		   1.0400411602399278,
		   1.0402758552053915,
		   1.0405106031319582,
		   1.0407454040315787,
		   1.0409802579162071,
		   1.0412151647977996,
		   1.0414501246883161,
		   1.0416851375997183,
		   1.0419202035439705,
		   1.0421553225330404,
		   1.042390494578898,
		   1.042625719693516,
		   1.0428609978888699,
		   1.043096329176938,
		   1.0433317135697009,
		   1.0435671510791424,
		   1.0438026417172486,
		   1.0440381854960086,
		   1.0442737824274138,
		   1.044509432523459,
		   1.044745135796141,
		   1.0449808922574599,
		   1.0452167019194181,
		   1.0454525647940205,
		   1.0456884808932754,
		   1.0459244502291931,
		   1.0461604728137874,
		   1.0463965486590741,
		   1.046632677777072,
		   1.0468688601798024,
		   1.0471050958792898,
		   1.047341384887561,
		   1.0475777272166455,
		   1.047814122878576,
		   1.048050571885387,
		   1.0482870742491166,
		   1.0485236299818055,
		   1.0487602390954964,
		   1.0489969016022356,
		   1.0492336175140715,
		   1.0494703868430555,
		   1.0497072096012419,
		   1.0499440858006872,
		   1.0501810154534512,
		   1.050417998571596,
		   1.0506550351671864,
		   1.0508921252522903,
		   1.0511292688389782,
		   1.0513664659393229,
		   1.0516037165654004,
		   1.0518410207292894,
		   1.0520783784430709,
		   1.0523157897188296,
		   1.0525532545686513,
		   1.0527907730046264,
		   1.0530283450388465,
		   1.0532659706834067,
		   1.0535036499504049,
		   1.0537413828519411,
		   1.0539791694001188,
		   1.0542170096070436,
		   1.0544549034848243,
		   1.0546928510455722,
		   1.0549308523014012,
		   1.0551689072644284,
		   1.0554070159467728,
		   1.0556451783605572,
		   1.0558833945179062,
		   1.0561216644309479,
		   1.0563599881118126,
		   1.0565983655726334,
		   1.0568367968255465,
		   1.0570752818826903,
		   1.0573138207562065,
		   1.057552413458239,
		   1.0577910600009348,
		   1.0580297603964437,
		   1.058268514656918,
		   1.0585073227945128,
		   1.0587461848213857,
		   1.058985100749698,
		   1.0592240705916123};

const double Opl3StereoMidiDriver::bend_coarse[128]
		= {1.0,
		   1.0594630943592953,
		   1.122462048309373,
		   1.189207115002721,
		   1.2599210498948732,
		   1.3348398541700344,
		   1.4142135623730951,
		   1.4983070768766815,
		   1.5874010519681994,
		   1.681792830507429,
		   1.7817974362806785,
		   1.8877486253633868,
		   2.0,
		   2.1189261887185906,
		   2.244924096618746,
		   2.3784142300054421,
		   2.5198420997897464,
		   2.6696797083400687,
		   2.8284271247461903,
		   2.996614153753363,
		   3.1748021039363992,
		   3.363585661014858,
		   3.5635948725613571,
		   3.7754972507267741,
		   4.0,
		   4.2378523774371812,
		   4.4898481932374912,
		   4.7568284600108841,
		   5.0396841995794928,
		   5.3393594166801366,
		   5.6568542494923806,
		   5.993228307506727,
		   6.3496042078727974,
		   6.727171322029716,
		   7.1271897451227151,
		   7.5509945014535473,
		   8.0,
		   8.4757047548743625,
		   8.9796963864749824,
		   9.5136569200217682,
		   10.079368399158986,
		   10.678718833360273,
		   11.313708498984761,
		   11.986456615013454,
		   12.699208415745595,
		   13.454342644059432,
		   14.25437949024543,
		   15.101989002907095,
		   16.0,
		   16.951409509748721,
		   17.959392772949972,
		   19.027313840043536,
		   20.158736798317967,
		   21.357437666720553,
		   22.627416997969522,
		   23.972913230026901,
		   25.398416831491197,
		   26.908685288118864,
		   28.508758980490853,
		   30.203978005814196,
		   32.0,
		   33.902819019497443,
		   35.918785545899944,
		   38.054627680087073,
		   40.317473596635935,
		   42.714875333441107,
		   45.254833995939045,
		   47.945826460053802,
		   50.796833662982394,
		   53.817370576237728,
		   57.017517960981706,
		   60.407956011628393,
		   64.0,
		   67.805638038994886,
		   71.837571091799887,
		   76.109255360174146,
		   80.63494719327187,
		   85.429750666882214,
		   90.509667991878089,
		   95.891652920107603,
		   101.59366732596479,
		   107.63474115247546,
		   114.03503592196341,
		   120.81591202325679,
		   128.0,
		   135.61127607798977,
		   143.67514218359977,
		   152.21851072034829,
		   161.26989438654374,
		   170.85950133376443,
		   181.01933598375618,
		   191.78330584021521,
		   203.18733465192958,
		   215.26948230495091,
		   228.07007184392683,
		   241.63182404651357,
		   256.0,
		   271.22255215597971,
		   287.35028436719938,
		   304.43702144069658,
		   322.53978877308765,
		   341.71900266752868,
		   362.03867196751236,
		   383.56661168043064,
		   406.37466930385892,
		   430.53896460990183,
		   456.14014368785394,
		   483.26364809302686,
		   512.0,
		   542.44510431195943,
		   574.70056873439876,
		   608.87404288139317,
		   645.0795775461753,
		   683.43800533505737,
		   724.07734393502471,
		   767.13322336086128,
		   812.74933860771785,
		   861.07792921980365,
		   912.28028737570787,
		   966.52729618605372,
		   1024.0,
		   1084.8902086239189,
		   1149.4011374687975,
		   1217.7480857627863,
		   1290.1591550923506,
		   1366.8760106701147,
		   1448.1546878700494,
		   1534.2664467217226};

// Read only the known SI ADLIB.ADV layout. No driver code or tables are bundled.
// Menu and introduction share the original classic mapping, but keep separate
// contexts: Guardian-only score enhancement is selected independently by Midi.cc.
bool Opl3StereoMidiDriver::setClassicContext(bool introduction,bool menu) {
	{auto lock=LockMutex();if(original_intro==introduction && original_menu==menu)return false;}
	if(!waitTillNoComMessages()) throw std::runtime_error("OPL3 Stereo: classic context command timeout");
	auto lock=LockMutex();original_intro=introduction;original_menu=menu;return true;
}
void Opl3StereoMidiDriver::loadIntroMapping() {
	// INTRO.EXE's validated classic mapping and initial part assignments are also
	// used by MAINMENU's original driver (verified against its local source).
	// Read original resources; do not embed the mapping or program tables.
	intro_channels.fill(0);intro_keys.fill(0);
	IFileDataSource exe("<STATIC>/../intro.exe");
	if(!exe.good() || exe.getSize()!=116068) throw std::runtime_error("unsupported BG INTRO.EXE for OPL3 intro mapping");
	std::vector<uint8> bytes(exe.getSize());exe.read(bytes.data(),bytes.size());
	if(std::string(sha1::toHexString(sha1::calc(bytes.data(),int(bytes.size()))).data())!="c8ea335ef7392865389c55f13b8ef1102fc4d427")
		throw std::runtime_error("unrecognized BG INTRO.EXE for OPL3 intro mapping");
	constexpr unsigned offsets[]={59199,59208,59216,59225,59233,59242,59251,59260,59269,59278,59287,59296,59305,59314,59323,59332,59341,59350,59359,59368,59377};
	for(unsigned i=0;i<intro_parts.size();++i) {
		const auto& part=bg_timbres[bytes[offsets[i]]];
		intro_parts[i]=part.native_bg?part:bg_default;
	}
	std::copy_n(bytes.begin()+111707,88,intro_keys.begin());
	std::copy_n(bytes.begin()+111619,88,intro_channels.begin());
}

void Opl3StereoMidiDriver::loadSiFrequencies() {
	try {
		IFileDataSource source("<STATIC>/adlib.adv");
		if(!source.good() || source.getSize()!=16144) throw std::runtime_error("unsupported ADLIB.ADV size");
		std::array<uint8,16144> bytes{};source.read(bytes.data(),bytes.size());
		uint64 fingerprint=14695981039346656037ULL;
		for(auto byte:bytes) fingerprint=(fingerprint^byte)*1099511628211ULL;
		if(fingerprint!=0xbc001cedaac90098ULL) throw std::runtime_error("unrecognized ADLIB.ADV version");
		for(unsigned i=0;i<si_frequencies.size();++i) si_frequencies[i]=bytes[2720+2*i]|(uint16(bytes[2721+2*i])<<8);
		si_bank=true;
	} catch(const std::exception& error) {
		std::cerr<<"OPL3 Stereo: SI native pitch/dynamics unavailable: "<<error.what()<<"; retaining generic FM behavior"<<std::endl;
	}
}

void Opl3StereoMidiDriver::updateSiPitch(int voice,int bend) {
	const auto& instrument=voice_timbre[voice];
	const int channel=chp[voice].channel;
	int note=(channel==9 ? int(instrument.transpose) : chp[voice].note+int(sint8(instrument.transpose)))-24;
	note+=12;while(note<0)note+=12;
	note+=12;do {note-=12;} while(note>95);
	auto floorDivide=[](int value,int divisor) {return value<0?-((-value+divisor-1)/divisor):value/divisor;};
	int pitch=floorDivide(floorDivide(bend-8192,32)*12+note*256+8,16)-192;
	pitch+=192;while(pitch<0)pitch+=192;
	pitch+=192;do {pitch-=192;} while(pitch>1535);
	int word=si_frequencies[(pitch/16%12)*16+(pitch&15)];
	int block=pitch/192-1;
	if(word&0x8000)++block;
	if(block<0) {++block;word=floorDivide(int(sint16(word)),2);}
	word=(word&1023)|(block<<10);
	midi_write_adlib(0xa0+channelRegister(voice),uint8(word));
	midi_write_adlib(0xb0+channelRegister(voice),uint8((word>>8)|32));
}

void Opl3StereoMidiDriver::midi_fm_playnote(int voice, int note, int volume, int pitchbend, int pan) {
	if(ownsEndingVoice(voice)) { ending_voices[voice].update|=1; updateEndingVoice(voice); return; }
	if (voice_timbre[voice].native_bg) {
		NukedStereo::OPLSetPan(opl,voice,voice_pan[voice]);
		updateBgPitch(voice); return;
	}
	if (voice_timbre[voice].native_si) {
		NukedStereo::OPLSetPan(opl,voice,voice_pan[voice]);
		midi_fm_volume(voice,volume); updateSiPitch(voice,pitchbend); return;
	}
	note=std::max(0,note);
	int freq = fnums[note % 12];
	int oct  = note / 12;

	pitchbend -= 0x2000;
	if (pitchbend != 0) {
		pitchbend *= 2;
		float pf;
		if (pitchbend >= 0) {
			pf = static_cast<float>(bend_fine[(pitchbend >> 5) & 0xFF] * bend_coarse[(pitchbend >> 13) & 0x7F]);
		} else {
			pitchbend = -pitchbend;
			pf        = static_cast<float>(1.0 / (bend_fine[(pitchbend >> 5) & 0xFF] * bend_coarse[(pitchbend >> 13) & 0x7F]));
		}
		freq = static_cast<int>(static_cast<float>(freq) * pf);

		while (freq >= (fnums[0] * 2)) {
			freq /= 2;
			oct += 1;
		}
		while (freq < fnums[0]) {
			freq *= 2;
			oct -= 1;
		}
	}

	NukedStereo::OPLSetPan(opl, voice, voice_pan[voice]);
	ignore_unused_variable_warning(pan);
	midi_fm_volume(voice, volume);
	midi_write_adlib(0xa0 + channelRegister(voice), static_cast<unsigned char>(freq & 0xff));

	oct=std::clamp(oct,0,7);
	const int c = ((freq & 0x300) >> 8) + (oct << 2) + (1 << 5);
	midi_write_adlib(0xb0 + channelRegister(voice), static_cast<unsigned char>(c));
}


void Opl3StereoMidiDriver::applyModulation(int voice) {
	if(ownsEndingVoice(voice)) {ending_voices[voice].update|=0x80;updateEndingVoice(voice);return;}
	const int channel=chp[voice].channel;
	if(channel<0 || voice_timbre[voice].native_bg) return;
	const auto& ins=voice_timbre[voice];
	const uint8 vibrato=ch[channel].modulation>=64?0x40:0;
	midi_write_adlib(0x20+adlib_opadd[voice],ins.mod_avekm|vibrato);
	midi_write_adlib(0x23+adlib_opadd[voice],ins.car_avekm|vibrato);
}

namespace {
int bgShift(int value,unsigned bits) {bits&=31;if(bits>15)return value<0?-1:0;int d=1<<bits;return value<0?-((-value+d-1)/d):value/d;}
int bgWord(const std::array<unsigned char,48>& data,unsigned at) {return sint16(data[at]|(data[at+1]<<8));}
}
void Opl3StereoMidiDriver::updateBgPitch(int voice) {
	const auto& state=bg_voice[voice]; if(state.channel<0)return;
	const auto& data=voice_timbre[voice].bg;const auto& channel=ch[state.channel];
	int relative=state.pitch-60*256;
	int keyShift=sint8(data[39]);relative=keyShift<0?bgShift(-relative,uint8(~data[39])):bgShift(relative,data[39]);
	unsigned phase=state.phase;int triangle=sint8(phase);
	if(triangle>63||triangle< -64)triangle=sint8(128-triangle);
	const auto& channelData=state.intro_part>=0 ? intro_parts[state.intro_part].bg : channel.ins.bg;
	const int depth=channelData[17]+bgShift(channel.modulation*channelData[15],7);
	int fixed=60*256+relative+bgShift((channel.pitchbend-8192)*channelData[14],8)+state.slide
		+bgShift(depth*triangle,4)+bgWord(data,36)+channel.nshift*256;
	int semitone=fixed/256,fraction=fixed%256;if(fraction<0){--semitone;fraction+=256;}
	int index=(semitone%12+12)%12;
	int freq=fnums[index]+((index==11?fnums[0]*2:fnums[index+1])-fnums[index])*fraction/256;
	int octave=semitone>=0?semitone/12:-((-semitone+11)/12);octave=std::clamp(octave,0,7);
	midi_write_adlib(0xa0+channelRegister(voice),uint8(freq));
	midi_write_adlib(0xb0+channelRegister(voice),uint8((freq>>8)|(octave<<2)|(state.keyed?32:0)));
}
void Opl3StereoMidiDriver::tickBgVoices() {
	for(int v=0;v<VoiceCount;++v) {
		auto& state=bg_voice[v];if(state.channel<0||!voice_timbre[v].native_bg)continue;
		const auto& data=voice_timbre[v].bg;
		unsigned at=state.stage<3?20+state.stage*4:state.stage==4?32:0;
		if(at){int rate=uint16(bgWord(data,at)),target=bgWord(data,at+2);int distance=std::abs(target-state.slide);
			if(distance<rate){state.slide=target;++state.stage;}else if(state.slide<target)state.slide+=rate;else state.slide-=rate;
		}
		const auto& root=state.intro_part>=0 ? intro_parts[state.intro_part] : ch[state.channel].ins;
		state.phase=(state.phase+root.bg[16])&255;
		updateBgPitch(v);
	}
}

void Opl3StereoMidiDriver::midi_fm_endnote(int voice) {
	ending_voices[voice]={};
	// Reuse the oldest release, rather than the oldest note-on: its envelope
	// has had the most time to decay before an instrument is reprogrammed.
	if (voice_timbre[voice].native_bg) { bg_voice[voice].keyed=false; bg_voice[voice].stage=4; }
	release_order[voice] = ++release_clock;
	voice_identity[voice] = 0;
	chp[voice].channel   = -1;
	chp[voice].note      = 0;
	chp[voice].sustained = false;
	chp[voice].velocity  = 0;
	midi_write_adlib(0xb0 + channelRegister(voice), static_cast<unsigned char>(adlib_data[0xb0 + channelRegister(voice)] & (255 - 32)));
}

void Opl3StereoMidiDriver::loadTimbreLibrary(IDataSource* ds, TimbreLibraryType type) {
	auto lock=LockMutex();
	for(unsigned v=0;v<VoiceCount;++v) if(ending_voices[v].status) midi_fm_endnote(v);
	ending_patches={}; ending_bank=false; ending_tick_phase=0; ending_volume_update=0;
	bg_bank = type==TIMBRE_LIBRARY_U7VOICE_AD;
	si_bank = false;
	if(type==TIMBRE_LIBRARY_XMIDI_AD) loadSiFrequencies();
	bg_timbres={};
	bg_default={};
	std::fill_n(timbre_valid,VoiceCount,false);
	// Clear the xmidibanks
	for (auto& xmidibank : xmidibanks) {
		delete xmidibank;
		xmidibank = nullptr;
	}

	for (auto& i : ch) {
		i.xmidi = false;
	}

	if (type == TIMBRE_LIBRARY_XMIDI_AD) {
		if(ending_context && si_bank && ds && ds->getSize()==2538) {
			std::vector<uint8> bytes(ds->getSize()); ds->seek(0); ds->read(bytes.data(),bytes.size()); ds->seek(0);
			ending_bank=std::string(sha1::toHexString(sha1::calc(bytes.data(),int(bytes.size()))).data())=="a1d5af1540a1ac23f259899bafd2b9013e9f449b";
		}
		if(ending_context && !ending_bank) std::cerr<<"OPL3 Stereo: unsupported Black Gate ending TVFX bank; retaining generic FM interpretation"<<std::endl;
		loadXMIDITimbres(ds);
	} else if (type == TIMBRE_LIBRARY_U7VOICE_AD) {
		loadU7VoiceTimbres(ds);
		if(original_intro || original_menu) loadIntroMapping();
	}
}

void Opl3StereoMidiDriver::loadXMIDITimbres(IDataSource* ds) {
	// Read all the timbres
	for (uint32 i = 0; ds->getAvail() > 0; i++) {
		// Seek to the entry
		ds->seek(i * 6);

		const uint32 patch = static_cast<uint8>(ds->read1());
		const uint32 bank  = static_cast<uint8>(ds->read1());

		// If we read both == 255 then we've read all of them
		if (patch == 255 || bank == 255) {
			// POUT ("Finished " << patch << ": ");
			break;
		}

		// Get offset and seek to it
		const uint32 offset = ds->read4();

		// POUT ("Patch " << i << " = " << bank << ":" << patch << " @ " <<
		// offset);

		// Check to see if the bank exists
		if (!xmidibanks[bank]) {
			xmidibanks[bank] = new xmidibank{};
		}

		// Seek to offset
		ds->seek(offset);

		// Get len of the entry
		const uint16 len = ds->read2();

		// The ending's seven validated definitions are parameter programs, not patches.
		if(ending_bank && bank==2 && (patch==53 || patch==54 || patch==58 || (patch>=60 && patch<=63))) {
			auto& bytes=ending_patches[patch]; bytes.resize(len); ds->seek(offset); ds->read(bytes.data(),len);
			auto& ins=xmidibanks[bank]->insbank[patch]; ins.tvfx_program=int(patch); ins.perc_voice=0x80;
			continue;
		}
		// Only accept lens that are 0xC
		if (len != 0xE) {
			POUT("Invalid Patch " << bank << ":" << patch << " len was " << len << " " << std::hex << len << std::dec);
			// continue;
		}

		// The original AIL bank stores transpose (or the percussion key) here.
		const uint8 transpose=ds->read1();

		struct {
			unsigned char mod_avekm;     // 0	(20)
			unsigned char mod_ksl_tl;    // 1	(40)
			unsigned char mod_ad;        // 2	(60)
			unsigned char mod_sr;        // 3	(80)
			unsigned char mod_ws;        // 4	(E0)

			unsigned char fb_c;    // 5

			unsigned char car_avekm;     // 6	amp, sussnd				(20)
			unsigned char car_ksl_tl;    // 7	outlev, keyscale		(43)
			unsigned char car_ad;        // 8	Attack Delay		AR	(63)
			unsigned char car_sr;        // 9	SustainLev Release	DR	(83)
			unsigned char car_ws;        // 10	Waveform				(E0)
		} xmidi_ins;

		ds->read(&xmidi_ins, 11);

		xinstrument& ins = xmidibanks[bank]->insbank[patch];

		ins.native_si = si_bank && len==0x0e;
		ins.transpose = transpose;
		ins.mod_avekm  = xmidi_ins.mod_avekm;
		ins.mod_ksl_tl = xmidi_ins.mod_ksl_tl;
		ins.mod_ad     = xmidi_ins.mod_ad;
		ins.mod_sr     = xmidi_ins.mod_sr;
		ins.mod_ws     = xmidi_ins.mod_ws;
		ins.car_avekm  = xmidi_ins.car_avekm;
		ins.car_ksl_tl = xmidi_ins.car_ksl_tl;
		ins.car_ad     = xmidi_ins.car_ad;
		ins.car_sr     = xmidi_ins.car_sr;
		ins.car_ws     = xmidi_ins.car_ws;
		ins.fb_c       = xmidi_ins.fb_c;
		ins.perc_voice = 0x80;    // Note that XMIDI uses a different method to U7:TBG
	}

	//	POUT ("Opl3StereoMidiDriver: " << i << " timbres read");

	for (auto& i : ch) {
		i.xmidi = true;
	}
}

void Opl3StereoMidiDriver::loadU7VoiceTimbres(IDataSource* ds) {
	struct u7voice_adlib_ins {
		unsigned char mod_avekm;     // 0: 	(20)
		unsigned char mod_ksl_tl;    // 1: 	(40)
		unsigned char mod_ad;        // 2: 	(60)
		unsigned char mod_sr;        // 3: 	(80)
		unsigned char mod_ws;        // 4: 	(E0)

		unsigned char car_avekm;     // 5: 	amp, sussnd				(22)
		unsigned char car_ksl_tl;    // 6: 	outlev, keyscale		(43)
		unsigned char car_ad;        // 7: 	Attack Delay		AR	(63)
		unsigned char car_sr;        // 8: 	SustainLev Release	DR	(83)
		unsigned char car_ws;        // 9: 	Waveform				(E3)

		unsigned char fb_c;    // 10:	Feedback/Connection

		// Native BG metadata follows the common OPL operator fields.

		unsigned char perc_voice;    // 11	Percussion voice number !!
									 // (0=melodic, 6=bass drum, etc.)

		unsigned char car_vel_sense;    // 12:  carrier velocity sensitivity
		unsigned char mod_vel_sense;    // 13:  modulator velocity sensitivity
		unsigned char bend_sense;       // 14:  pitch bend sensitivity
		unsigned char wheel_sense;      // 15:  modulation wheel sensitivity
		unsigned char lfo_speed;        // 16:  lfo speed
		unsigned char lfo_depth;        // 17:  lfo depth

		unsigned short pe_start_level;      // 18-19:  pitch envelope start level
		unsigned short pe_attack_rate1;     // 20-21:  pitch envelope attack rate 1
		unsigned short pe_attack_level1;    // 22-23:  pitch envelope attack level 1
		unsigned short pe_attack_rate2;     // 24-25:  pitch envelope attack rate 2
		unsigned short pe_attack_level2;    // 26-27:  pitch envelope attack level 2
		unsigned short pe_decay_rate;       // 28-29:  pitch envelope decay rate
		unsigned short pe_sustain_level;    // 30-31:  pitch envelope sustain level
		unsigned short pe_release_rate;     // 32-33:  pitch envelope release rate
		unsigned short pe_end_level;        // 34-35:  pitch envelope end level

		unsigned char deturn;          // 36:  detune
		unsigned char transpose;       // 37:  transpose
		unsigned char next_partial;    // 38:  next partial number (+1; 0=no
									   // more partials)
		unsigned char key_follow;      // 39:  key follow
		unsigned char reserved[7];     // 40-46:  reserved

		unsigned char prog_num;    // 47:  program change number

		void read(IDataSource* ds) {
			mod_avekm  = ds->read1();
			mod_ksl_tl = ds->read1();
			mod_ad     = ds->read1();
			mod_sr     = ds->read1();
			mod_ws     = ds->read1();
			car_avekm  = ds->read1();
			car_ksl_tl = ds->read1();
			car_ad     = ds->read1();
			car_sr     = ds->read1();
			car_ws     = ds->read1();
			fb_c       = ds->read1();

			// Native BG metadata follows the common OPL operator fields.

			perc_voice = ds->read1();

			car_vel_sense = ds->read1();
			mod_vel_sense = ds->read1();
			bend_sense    = ds->read1();
			wheel_sense   = ds->read1();
			lfo_speed     = ds->read1();
			lfo_depth     = ds->read1();

			pe_start_level   = ds->read2();
			pe_attack_rate1  = ds->read2();
			pe_attack_level1 = ds->read2();
			pe_attack_rate2  = ds->read2();
			pe_attack_level2 = ds->read2();
			pe_decay_rate    = ds->read2();
			pe_sustain_level = ds->read2();
			pe_release_rate  = ds->read2();
			pe_end_level     = ds->read2();

			deturn       = ds->read1();
			transpose    = ds->read1();
			next_partial = ds->read1();
			key_follow   = ds->read1();
			ds->read(reinterpret_cast<char*>(reserved), 7);

			prog_num = ds->read1();
		}
	} u7voice_ins;

	// POUT("Size of u7voice_adlib_ins " << sizeof(u7voice_adlib_ins));

	const int count = ds->read1() & 0xFF;

	// Read all the timbres
	for (int i = 0; i < count; i++) {
		// Read the timbre
		u7voice_ins.read(ds);

		const uint32 patch = u7voice_ins.prog_num;
		const uint32 bank  = 0;

		// POUT ("Patch " << i << " = " << bank << ":" << patch);

		// Check to see if the bank exists
		if (!xmidibanks[bank]) {
			xmidibanks[bank] = new xmidibank{};
		}

		xinstrument& ins = bg_timbres[patch];

		ins.mod_avekm  = u7voice_ins.mod_avekm;
		ins.mod_ksl_tl = u7voice_ins.mod_ksl_tl;
		ins.mod_ad     = u7voice_ins.mod_ad;
		ins.mod_sr     = u7voice_ins.mod_sr;
		ins.mod_ws     = u7voice_ins.mod_ws;
		ins.car_avekm  = u7voice_ins.car_avekm;
		ins.car_ksl_tl = u7voice_ins.car_ksl_tl;
		ins.car_ad     = u7voice_ins.car_ad;
		ins.car_sr     = u7voice_ins.car_sr;
		ins.car_ws     = u7voice_ins.car_ws;
		ins.fb_c       = u7voice_ins.fb_c;
		ins.perc_voice = u7voice_ins.perc_voice | 0x80;    // Note that XMIDI uses a different method
														   // to U7:TBG
		ins.native_bg=true;
		ins.bg[12]=u7voice_ins.car_vel_sense; ins.bg[13]=u7voice_ins.mod_vel_sense;
		ins.bg[14]=u7voice_ins.bend_sense; ins.bg[15]=u7voice_ins.wheel_sense;
		ins.bg[16]=u7voice_ins.lfo_speed; ins.bg[17]=u7voice_ins.lfo_depth;
		const unsigned short envelope[]={u7voice_ins.pe_start_level,u7voice_ins.pe_attack_rate1,u7voice_ins.pe_attack_level1,
			u7voice_ins.pe_attack_rate2,u7voice_ins.pe_attack_level2,u7voice_ins.pe_decay_rate,u7voice_ins.pe_sustain_level,
			u7voice_ins.pe_release_rate,u7voice_ins.pe_end_level};
		for(unsigned j=0;j<9;++j){ins.bg[18+j*2]=uint8(envelope[j]);ins.bg[19+j*2]=uint8(envelope[j]>>8);}
		ins.bg[36]=u7voice_ins.deturn;ins.bg[37]=u7voice_ins.transpose;
		ins.bg[38]=u7voice_ins.next_partial;ins.bg[39]=u7voice_ins.key_follow;ins.bg[47]=uint8(patch);
		if(i==0) bg_default=ins; // DOS FindTimbre falls back to the first bank record.
		if(patch<128) xmidibanks[bank]->insbank[patch]=ins;


		if (u7voice_ins.next_partial || u7voice_ins.key_follow) {
			//	POUT("next_partial " << (int) u7voice_ins.next_partial << "
			// key_follow " << (int) u7voice_ins.key_follow);
		}
	}

	// POUT (i << " timbres read");

	for (auto& i : ch) {
		i.xmidi = true;
	}
}



// Black Gate ending TVFX: adapted from the accepted GPL OriginalOpl2Ending
// interpreter. Scheduling and physical-voice selection remain those of OPL3.
void Opl3StereoMidiDriver::finishSequence(int sequence) {
	LowLevelMidiDriver::finishSequence(sequence);
	{auto lock=LockMutex();if(sequence!=0 || !ending_context)return;}
	if(!waitTillNoComMessages()) std::cerr<<"OPL3 Stereo: ending stop command timeout"<<std::endl;
	auto lock=LockMutex();
	// A naturally completed sequencer may already be gone while TVFX is releasing.
	for(unsigned v=0;v<VoiceCount;++v) if(ending_voices[v].status)midi_fm_endnote(v);
}
bool Opl3StereoMidiDriver::setEndingContext(bool requested,IDataSource* score,unsigned sequence) {
	{auto lock=LockMutex();if(!requested && !ending_context)return false;}
	bool enabled=false;
	if(requested && score && (sequence==3 || sequence==4) && score->getSize()==18346) {
		const auto position=score->getPos(); std::vector<uint8> bytes(score->getSize());
		score->seek(0); score->read(bytes.data(),bytes.size()); score->seek(position);
		enabled=std::string(sha1::toHexString(sha1::calc(bytes.data(),int(bytes.size()))).data())=="5b364cd61b2c431b4f8ddd835652fb7447d7e5db";
	}
	if(requested && !enabled) std::cerr<<"OPL3 Stereo: unsupported Black Gate ending score; TVFX correction not activated"<<std::endl;
	// Drain the previous score's queued finish before changing bank context.
	if(!waitTillNoComMessages()) throw std::runtime_error("OPL3 Stereo: ending context command timeout");
	auto lock=LockMutex();
	if(enabled==ending_context) return false;
	for(unsigned v=0;v<VoiceCount;++v) if(ending_voices[v].status) midi_fm_endnote(v);
	ending_context=enabled; ending_bank=false; ending_patches={}; ending_tick_phase=0;
	return true;
}
bool Opl3StereoMidiDriver::ownsEndingVoice(unsigned v) const {
	const auto& s=ending_voices[v];
	return s.status && chp[v].channel==s.channel && voice_identity[v]==s.identity;
}
unsigned Opl3StereoMidiDriver::endingByte(const EndingVoice& s,unsigned at) const {
	const auto& bytes=ending_patches[s.program]; return at<bytes.size()?bytes[at]:0;
}
uint16 Opl3StereoMidiDriver::endingWord(const EndingVoice& s,unsigned at) const {
	return uint16(endingByte(s,at)|(endingByte(s,uint16(at+1))<<8));
}
void Opl3StereoMidiDriver::endingPhase(EndingVoice& s) {
	s.feedback=0; s.ksl={}; s.avekm={32,32}; s.scale=2; s.type=endingByte(s,3); s.block=s.type==1?32:40;
	uint16 e0=0xff0f,e1=0xff0f;
	if(uint16(endingWord(s,8)+2)!=54) {
		e0=endingWord(s,s.status==2?58:54); e1=endingWord(s,s.status==2?60:56);
	}
	s.ad={uint8(e1>>8),uint8(e0>>8)}; s.sr={uint8(e1),uint8(e0)};
	if(s.status!=2) s.duration=s.type==1?65535:uint16(endingWord(s,4)+1);
	for(unsigned p=0;p<8;++p) {
		unsigned at=6+6*p;
		if(s.status!=2) s.value[p]=endingWord(s,at);
		s.position[p]=uint16(endingWord(s,at+(s.status==2?4:2))+2); s.count[p]=1; s.step[p]=0;
	}
	s.update=0xf9;
}
void Opl3StereoMidiDriver::endingFetch(EndingVoice& s,unsigned p) {
	for(unsigned n=0;n<10;++n) {
		unsigned at=s.position[p]; uint16 op=endingWord(s,at),operand=endingWord(s,uint16(at+2));
		if(!op) {s.position[p]+=operand;continue;}
		s.position[p]+=4;
		if(op==65535) {s.value[p]=operand;continue;}
		if(op!=65534) {s.count[p]=op;s.step[p]=operand;return;}
		if(p==5 || p==6) s.avekm[p-5]=uint8(operand);
		else if(p==1 || p==2) s.ksl[p-1]=uint8(operand);
		else if(p==0) {s.block=uint8(operand>>8);if(s.type==1)s.block&=0xe0;}
		else if(p==4) {s.feedback=uint8(operand>>8);s.scale=uint8((operand>>8)|2);}
	}
	s.step[p]=0;s.count[p]=65535;
}
void Opl3StereoMidiDriver::startEndingVoice(unsigned v,int program,unsigned channel,unsigned key,unsigned velocity) {
	auto& s=ending_voices[v]; s={};s.program=program;s.identity=voice_identity[v];
	s.channel=uint8(channel);s.key=uint8(key);s.transpose=endingByte(s,2);
	s.velocity=uint8(82+3*(velocity>>3));s.status=1;
	// TVFX writes all operator parameters itself; invalidate the ordinary cache.
	timbre_valid[v]=false;voice_timbre[v]={};bg_voice[v]={};
	NukedStereo::OPLSetPan(opl,v,voice_pan[v]);
	endingPhase(s);updateEndingVoice(v);
}
void Opl3StereoMidiDriver::releaseEndingVoice(unsigned v) {
	if(ownsEndingVoice(v) && ending_voices[v].status==1) ending_voices[v].duration=1;
}
void Opl3StereoMidiDriver::updateEndingVoice(unsigned v) {
	if(!ownsEndingVoice(v)) return;
	auto& s=ending_voices[v];const auto& channel=ch[s.channel];
	unsigned m=adlib_opadd[v],c=m+3,r=channelRegister(v);
	auto put=[&](unsigned reg,uint8 value) {if(adlib_data[reg]!=value)midi_write_adlib(reg,value);};
	auto scaled=[](unsigned x,unsigned y) {unsigned result=uint16((x*y)<<1)>>8;return result?result+1:0;};
	unsigned gain=scaled(scaled(channel.vol,channel.expression),s.velocity);
	if(s.update&0x80) {
		unsigned vib=channel.modulation>=64?64:0;
		put(0x20+m,uint8((s.value[5]>>12)|vib|s.avekm[0]));
		put(0x20+c,uint8((s.value[6]>>12)|vib|s.avekm[1]));
	}
	if(s.update&0x40) for(unsigned op=0;op<2;++op) {
		unsigned level=s.value[op+1]>>10;
		if(s.scale&(1<<op))level=uint8(level*gain/127);
		put(0x40+(op?c:m),uint8((~level&63)|s.ksl[op]));
	}
	if(s.update&0x20) {put(0x60+m,s.ad[0]);put(0x60+c,s.ad[1]);put(0x80+m,s.sr[0]);put(0x80+c,s.sr[1]);}
	if(s.update&0x10) {put(0xe0+c,uint8(s.value[7]));put(0xe0+m,uint8(s.value[7]>>8));}
	if(s.update&8) put(0xc0+r,uint8(((s.value[4]>>12)&14)|s.feedback));
	if(s.update&1) {
		unsigned frequency;
		if(s.type==2)frequency=s.value[0]>>6;
		else if(!(s.block&32)) {put(0xb0+r,s.key_shadow&~32);s.update=0;return;}
		else {
			int bend=((channel.pitchbend-8192)>>5)*12;
			int note=int(s.key)+sint8(s.transpose)-24;
			do {note+=12;}while(note<0);
			note+=12;do {note-=12;}while(note>95);
			int pitch=sint16(sint16(bend+(note<<8))+8)>>4;
			pitch-=192;do {pitch+=192;}while(pitch<0);
			pitch+=192;do {pitch-=192;}while(pitch>1535);
			int block=pitch/192-1;uint16 raw=si_frequencies[((pitch>>4)%12)*16+(pitch&15)];
			if(sint16(raw)<0)++block;
			if(block<0){++block;raw=uint16(sint16(raw)>>1);}
			frequency=(raw&1023)|(uint8(block<<2)<<8);
		}
		put(0xa0+r,uint8(frequency));s.key_shadow=uint8((frequency>>8)|s.block);put(0xb0+r,s.key_shadow);
	}
	s.update=0;
}
void Opl3StereoMidiDriver::tickEndingVoices() {
	ending_volume_update^=0x40;
	for(unsigned v=0;v<VoiceCount;++v) {
		auto& s=ending_voices[v];
		if(!ownsEndingVoice(v)) {s={};continue;}
		auto step=[&](unsigned p,uint8 flag,bool level=false) {
			if(s.step[p]) {
				uint16 old=s.value[p],now=uint16(old+s.step[p]);s.value[p]=now;
				if(level && s.status==2 && ((now^old)&0x8000) && !((now^s.step[p])&0x8000))s.value[p]=0;
				s.update|=flag;
			}
			if(--s.count[p]==0) {endingFetch(s,p);s.update|=level?0x40:flag;}
		};
		step(0,1);step(4,8);step(5,0x80);step(6,0x80);
		step(1,ending_volume_update,true);step(2,ending_volume_update,true);step(7,0x10);
		s.value[3]+=s.step[3];if(--s.count[3]==0)endingFetch(s,3);
		if(s.update&0xf9)updateEndingVoice(v);
		if(s.status!=2) {if(--s.duration==0){s.status=2;endingPhase(s);}}
		else if(s.value[1]<1024 && s.value[2]<1024)midi_fm_endnote(v);
	}
}

#endif // USE_FMOPL_MIDI
