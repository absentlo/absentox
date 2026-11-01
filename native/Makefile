#!/usr/bin/make -f
# Builds the VST3 and CLAP plugins into native/build/. Run: make -C native

NAME = ABSENToX
DPF_PATH = dpf
DPF_BUILD_DIR = build/obj
DPF_TARGET_DIR = build

FILES_DSP = AbsentoxPlugin.cpp
FILES_UI = AbsentoxUI.cpp

include dpf/Makefile.plugins.mk

all: dgl vst3 clap

dgl:
	$(MAKE) -C dpf/dgl opengl

.PHONY: all dgl
