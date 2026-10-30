// Exult music positioning metadata transfer. GPL-2.0-or-later.
#ifndef EXULT_OPL3_MUSIC_PAN_H
#define EXULT_OPL3_MUSIC_PAN_H
class XMidiEventList;
// Transfers only pan metadata for corresponding existing notes. Never adds notes.
unsigned transferOpl3MusicPan(XMidiEventList* playback, XMidiEventList* fm, XMidiEventList* roland);
// Adds only validated source MT-32 notes missing from an aligned FM channel.
unsigned recoverOpl3MusicNotes(XMidiEventList* playback, XMidiEventList* fm, XMidiEventList* roland);
#endif
