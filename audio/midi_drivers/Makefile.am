if BUILD_MT32EMU
MT32EMUFLAGS=-DMT32EMU_EXPORTS_TYPE=0 -DMT32EMU_API_TYPE=0
MT32EMUDRV=	\
	MT32EmuMidiDriver.cpp	\
	MT32EmuMidiDriver.h
endif

AM_CPPFLAGS = -I$(top_srcdir)/headers -I$(top_srcdir)/conf -I$(top_srcdir) \
		-I$(top_srcdir)/audio -I$(top_srcdir)/files  -I$(top_srcdir)/gumps -I$(top_srcdir)/imagewin \
		-I$(top_srcdir)/objs -I$(top_srcdir)/shapes \
		$(SDL_CFLAGS) $(MT32EMU_CFLAGS) $(ALSA_CFLAGS) $(FLUID_CFLAGS) $(INCDIRS) $(WINDOWING_SYSTEM) \
		$(MT32EMUFLAGS) \
		$(DEBUG_LEVEL) $(OPT_LEVEL) $(WARNINGS) $(CPPFLAGS)

SUBDIRS = timidity

noinst_LTLIBRARIES = libmididrv.la

libmididrv_la_SOURCES =	\
	ALSAMidiDriver.cpp	\
	ALSAMidiDriver.h	\
	CoreAudioMidiDriver.cpp	\
	CoreAudioMidiDriver.h	\
	CoreMidiDriver.cpp \
	CoreMidiDriver.h \
	fmopl.cpp		\
	fmopl.h			\
	FMOplMidiDriver.cpp	\
	FMOplMidiDriver.h	\
	OriginalOpl2MidiDriver.cpp \
	OriginalOpl2MidiDriver.h \
	OriginalOpl2Ending.cpp \
	OriginalOpl2Serpent.cpp \
	Opl3StereoMidiDriver.cpp \
	Opl3StereoMidiDriver.h \
	NukedOpl3.cpp \
	NukedOpl3.h \
	Opl3MusicPan.cpp \
	Opl3MusicPan.h \
	LowLevelMidiDriver.cpp	\
	LowLevelMidiDriver.h	\
	MidiDriver.cpp		\
	MidiDriver.h		\
	$(MT32EMUDRV)	\
	TimidityMidiDriver.cpp	\
	TimidityMidiDriver.h	\
	UnixSeqMidiDriver.cpp	\
	UnixSeqMidiDriver.h	\
	WindowsMidiDriver.cpp	\
	WindowsMidiDriver.h	\
	XMidiEvent.h		\
	XMidiEventList.cpp	\
	XMidiEventList.h	\
	XMidiFile.cpp		\
	XMidiFile.h		\
	XMidiNoteStack.h	\
	XMidiRecyclable.h \
	XMidiSequence.cpp	\
	XMidiSequence.h		\
	XMidiSequenceHandler.h	\
	FluidSynthMidiDriver.cpp	\
	FluidSynthMidiDriver.h

EXTRA_DIST = 		\
	README \
	nuked-opl3/opl3.c \
	nuked-opl3/opl3.h \
	nuked-opl3/LICENSE \
	nuked-opl3/README.exult

CLEANFILES = *~
