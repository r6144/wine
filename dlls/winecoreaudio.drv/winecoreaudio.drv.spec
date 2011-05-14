# WinMM driver functions
@ stdcall -private DriverProc(long long long long long) CoreAudio_DriverProc
@ stdcall -private widMessage(long long long long long) CoreAudio_widMessage
@ stdcall -private wodMessage(long long long long long) CoreAudio_wodMessage
@ stdcall -private midMessage(long long long long long) CoreAudio_midMessage
@ stdcall -private modMessage(long long long long long) CoreAudio_modMessage
@ stdcall -private mxdMessage(long long long long long) CoreAudio_mxdMessage

# MMDevAPI driver functions
@ stdcall -private GetEndpointIDs(long ptr ptr ptr) AUDDRV_GetEndpointIDs
@ stdcall -private GetAudioEndpoint(str long ptr) AUDDRV_GetAudioEndpoint
