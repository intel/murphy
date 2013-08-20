/*
 * Copyright (c) 2012, 2013, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __MURPHY_DBUS_ERROR_H__
#define __MURPHY_DBUS_ERROR_H__

#define __ERR(error) "org.freedesktop.DBus.Error."#error

#define MRP_DBUS_ERROR_FAILED                     __ERR(Failed)
#define MRP_DBUS_ERROR_NO_MEMORY                  __ERR(NoMemory)
#define MRP_DBUS_ERROR_SERVICE_UNKNOWN            __ERR(ServiceUnknown)
#define MRP_DBUS_ERROR_NAME_HAS_NO_OWNER          __ERR(NameHasNoOwner)
#define MRP_DBUS_ERROR_NO_REPLY                   __ERR(NoReply)
#define MRP_DBUS_ERROR_IO_ERROR                   __ERR(IOError)
#define MRP_DBUS_ERROR_BAD_ADDRESS                __ERR(BadAddress)
#define MRP_DBUS_ERROR_NOT_SUPPORTED              __ERR(NotSupported)
#define MRP_DBUS_ERROR_LIMITS_EXCEEDED            __ERR(LimitsExceeded)
#define MRP_DBUS_ERROR_ACCESS_DENIED              __ERR(AccessDenied)
#define MRP_DBUS_ERROR_AUTH_FAILED                __ERR(AuthFailed)
#define MRP_DBUS_ERROR_NO_SERVER                  __ERR(NoServer)
#define MRP_DBUS_ERROR_TIMEOUT                    __ERR(Timeout)
#define MRP_DBUS_ERROR_NO_NETWORK                 __ERR(NoNetwork)
#define MRP_DBUS_ERROR_ADDRESS_IN_USE             __ERR(AddressInUse)
#define MRP_DBUS_ERROR_DISCONNECTED               __ERR(Disconnected)
#define MRP_DBUS_ERROR_INVALID_ARGS               __ERR(InvalidArgs)
#define MRP_DBUS_ERROR_FILE_NOT_FOUND             __ERR(FileNotFound)
#define MRP_DBUS_ERROR_FILE_EXISTS                __ERR(FileExists)
#define MRP_DBUS_ERROR_UNKNOWN_METHOD             __ERR(UnknownMethod)
#define MRP_DBUS_ERROR_UNKNOWN_OBJECT             __ERR(UnknownObject)
#define MRP_DBUS_ERROR_UNKNOWN_INTERFACE          __ERR(UnknownInterface)
#define MRP_DBUS_ERROR_UNKNOWN_PROPERTY           __ERR(UnknownProperty)
#define MRP_DBUS_ERROR_PROPERTY_READ_ONLY         __ERR(PropertyReadOnly)
#define MRP_DBUS_ERROR_TIMED_OUT                  __ERR(TimedOut)
#define MRP_DBUS_ERROR_MATCH_RULE_NOT_FOUND       __ERR(MatchRuleNotFound)
#define MRP_DBUS_ERROR_MATCH_RULE_INVALID         __ERR(MatchRuleInvalid)
#define MRP_DBUS_ERROR_SPAWN_EXEC_FAILED          __ERR(Spawn.ExecFailed)
#define MRP_DBUS_ERROR_SPAWN_FORK_FAILED          __ERR(Spawn.ForkFailed)
#define MRP_DBUS_ERROR_SPAWN_CHILD_EXITED         __ERR(Spawn.ChildExited)
#define MRP_DBUS_ERROR_SPAWN_CHILD_SIGNALED       __ERR(Spawn.ChildSignaled)
#define MRP_DBUS_ERROR_SPAWN_FAILED               __ERR(Spawn.Failed)
#define MRP_DBUS_ERROR_SPAWN_SETUP_FAILED         __ERR(Spawn.FailedToSetup)
#define MRP_DBUS_ERROR_SPAWN_CONFIG_INVALID       __ERR(Spawn.ConfigInvalid)
#define MRP_DBUS_ERROR_SPAWN_SERVICE_INVALID      __ERR(Spawn.ServiceNotValid)
#define MRP_DBUS_ERROR_SPAWN_SERVICE_NOT_FOUND    __ERR(Spawn.ServiceNotFound)
#define MRP_DBUS_ERROR_SPAWN_PERMISSIONS_INVALID  __ERR(Spawn.PermissionsInvalid)
#define MRP_DBUS_ERROR_SPAWN_FILE_INVALID         __ERR(Spawn.FileInvalid)
#define MRP_DBUS_ERROR_SPAWN_NO_MEMORY            __ERR(Spawn.NoMemory)
#define MRP_DBUS_ERROR_UNIX_PROCESS_ID_UNKNOWN    __ERR(UnixProcessIdUnknown)
#define MRP_DBUS_ERROR_INVALID_SIGNATURE          __ERR(InvalidSignature)
#define MRP_DBUS_ERROR_INVALID_FILE_CONTENT       __ERR(InvalidFileContent)
#define MRP_DBUS_ERROR_ADT_AUDIT_DATA_UNKNOWN     __ERR(AdtAuditDataUnknown)
#define MRP_DBUS_ERROR_OBJECT_PATH_IN_USE         __ERR(ObjectPathInUse)
#define MRP_DBUS_ERROR_INCONSISTENT_MESSAGE       __ERR(InconsistentMessage)
#define MRP_DBUS_ERROR_SELINUX_SECURITY_CONTEXT_UNKNOWN __ERR(SELinuxSecurityContextUnknown)

#endif /* __MURPHY_MRP_DBUS_ERROR_H__ */
