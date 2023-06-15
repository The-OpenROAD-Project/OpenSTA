// OpenSTA, Static Timing Analyzer
// Copyright (c) 2023, Parallax Software, Inc.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include "ReportTcl.hh"

#include <cstdio>
#include <cstdlib>

namespace sta {

using ::ClientData;
using ::Tcl_Channel;
using ::Tcl_ChannelOutputProc;
using ::Tcl_ChannelType;
using ::Tcl_DriverOutputProc;
using ::Tcl_GetChannelInstanceData;
using ::Tcl_GetChannelType;

extern "C" {

static int
encapOutputProc(ClientData instanceData,
                CONST84 char *buf,
                int toWrite,
                int *errorCodePtr);
static int
encapCloseProc(ClientData instanceData, Tcl_Interp *interp);
static int
encapSetOptionProc(ClientData instanceData,
                   Tcl_Interp *interp,
                   CONST84 char *optionName,
                   CONST84 char *value);
static int
encapGetOptionProc(ClientData instanceData,
                   Tcl_Interp *interp,
                   CONST84 char *optionName,
                   Tcl_DString *dsPtr);
static int
encapInputProc(ClientData instanceData,
               char *buf,
               int bufSize,
               int *errorCodePtr);
static int
encapSeekProc(ClientData instanceData,
              long offset,
              int seekMode,
              int *errorCodePtr);
static void
encapWatchProc(ClientData instanceData, int mask);
static int
encapGetHandleProc(ClientData instanceData,
                   int direction,
                   ClientData *handlePtr);
static int
encapBlockModeProc(ClientData instanceData, int mode);
}  // extern "C"

Tcl_ChannelType tcl_encap_type_stdout = {
    const_cast<char *>("file"),
    TCL_CHANNEL_VERSION_4,
    encapCloseProc,
    encapInputProc,
    encapOutputProc,
    encapSeekProc,
    encapSetOptionProc,
    encapGetOptionProc,
    encapWatchProc,
    encapGetHandleProc,
    nullptr,  // close2Proc
    encapBlockModeProc,
    nullptr,  // flushProc
    nullptr,  // handlerProc
    nullptr,  // wideSeekProc
    nullptr,  // threadActionProc
    nullptr   // truncateProc
};

////////////////////////////////////////////////////////////////

ReportTcl::ReportTcl() :
    Report(), interp_(nullptr),
    tcl_stdout_(nullptr),
    tcl_stderr_(nullptr),
    tcl_encap_stdout_(nullptr),
    tcl_encap_stderr_(nullptr)
{
}

ReportTcl::~ReportTcl()
{
  tcl_encap_stdout_ = nullptr;
  tcl_encap_stderr_ = nullptr;
  Tcl_UnstackChannel(interp_, tcl_stdout_);
  Tcl_UnstackChannel(interp_, tcl_stderr_);
}

void
ReportTcl::setTclInterp(Tcl_Interp *interp)
{
  interp_ = interp;
  tcl_stdout_ = Tcl_GetStdChannel(TCL_STDOUT);
  tcl_stderr_ = Tcl_GetStdChannel(TCL_STDERR);
  tcl_encap_stdout_ = Tcl_StackChannel(interp,
                                       &tcl_encap_type_stdout,
                                       this,
                                       TCL_WRITABLE,
                                       tcl_stdout_);
  tcl_encap_stderr_ = Tcl_StackChannel(interp,
                                       &tcl_encap_type_stdout,
                                       this,
                                       TCL_WRITABLE,
                                       tcl_stderr_);
}

size_t
ReportTcl::printConsole(const char *buffer,
                        size_t length)
{
  return printTcl(tcl_stdout_, buffer, length);
}

size_t
ReportTcl::printTcl(Tcl_Channel channel,
                    const char *buffer,
                    size_t length)
{
  const Tcl_ChannelType *ch_type = Tcl_GetChannelType(channel);
  Tcl_DriverOutputProc *output_proc = Tcl_ChannelOutputProc(ch_type);
  int error_code;
  ClientData clientData = Tcl_GetChannelInstanceData(channel);
  return output_proc(clientData,
                     const_cast<char *>(buffer),
                     length,
                     &error_code);
}

void
ReportTcl::flush()
{
  if (tcl_encap_stdout_)
    Tcl_Flush(tcl_encap_stdout_);
  if (tcl_encap_stderr_)
    Tcl_Flush(tcl_encap_stderr_);
}

// Tcl_Main can eval multiple commands before the flushing the command
// output, so the log/redirect commands must force a flush.
void
ReportTcl::logBegin(const char *filename)
{
  flush();
  Report::logBegin(filename);
}

void
ReportTcl::logEnd()
{
  flush();
  Report::logEnd();
}

void
ReportTcl::redirectFileBegin(const char *filename)
{
  flush();
  Report::redirectFileBegin(filename);
}

void
ReportTcl::redirectFileAppendBegin(const char *filename)
{
  flush();
  Report::redirectFileAppendBegin(filename);
}

void
ReportTcl::redirectFileEnd()
{
  flush();
  Report::redirectFileEnd();
}

void
ReportTcl::redirectStringBegin()
{
  flush();
  Report::redirectStringBegin();
}

const char *
ReportTcl::redirectStringEnd()
{
  flush();
  return Report::redirectStringEnd();
}

////////////////////////////////////////////////////////////////

static int
encapOutputProc(ClientData instanceData,
                CONST84 char *buf,
                int toWrite,
                int *)
{
  ReportTcl *report = reinterpret_cast<ReportTcl *>(instanceData);
  return report->printString(buf, toWrite);
}

static int
encapInputProc(ClientData,
               char *,
               int,
               int *)
{
  return -1;
}

static int
encapCloseProc(ClientData instanceData,
               Tcl_Interp *)
{
  ReportTcl *report = reinterpret_cast<ReportTcl *>(instanceData);
  report->logEnd();
  report->redirectFileEnd();
  report->redirectStringEnd();
  return 0;
}

static int
encapSetOptionProc(ClientData,
                   Tcl_Interp *,
                   CONST84 char *,
                   CONST84 char *)
{
  return 0;
}

static int
encapGetOptionProc(ClientData,
                   Tcl_Interp *,
                   CONST84 char *,
                   Tcl_DString *)
{
  return 0;
}

static int
encapSeekProc(ClientData,
              long,
              int,
              int *)
{
  return -1;
}

static void
encapWatchProc(ClientData, int)
{
}

static int
encapGetHandleProc(ClientData,
                   int,
                   ClientData *)
{
  return TCL_ERROR;
}

static int
encapBlockModeProc(ClientData,
                   int)
{
  return 0;
}

}  // namespace sta
