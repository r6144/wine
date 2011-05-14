#include "config.h"
#include "wine/port.h"

#include "dibdrv.h"

WINE_DEFAULT_DEBUG_CHANNEL(dibdrv);

/* the ROP3 operations
   this is a BIG case block; beware that some
   commons ROP3 operations will be optimized
   from inside blt routines */
DWORD _DIBDRV_ROP3(DWORD p, DWORD s, DWORD d, BYTE rop)
{
    switch(rop)
    {
        case 0x00:   /* 0           BLACKNESS    */
            return 0;
            
        case 0x01:   /* DPSoon                   */
            return (~(p | s | d)) & 0x00ffffff;
            
        case 0x02:   /* DPSona                   */
            return (d & ~(p | s)) & 0x00ffffff;
            
        case 0x03:   /* PSon                     */
            return (~(p | s)) & 0x00ffffff;
            
        case 0x04:   /* SDPona                   */
            return (s & ~(d | p)) & 0x00ffffff;
            
        case 0x05:   /* DPon                     */
            return (~(d | p)) & 0x00ffffff;
            
        case 0x06:   /* PDSxnon                  */
            return (~(p | ~(d ^ s))) & 0x00ffffff;
            
        case 0x07:   /* PDSaon                   */
            return (~((d & s) | p) ) & 0x00ffffff;
            
        case 0x08:   /* SDPnaa                   */
            return (d & ~p & s) & 0x00ffffff;
            
        case 0x09:   /* PDSxon                   */
            return (~((d ^ s) | p)) & 0x00ffffff;
            
        case 0x0A:   /* DPna                     */
            return (~p & d) & 0x00ffffff;
            
        case 0x0B:   /* PSDnaon                  */
            return (~((~d & s) | p)) & 0x00ffffff;
            
        case 0x0C:   /* SPna                     */
            return (~p & s) & 0x00ffffff;
            
        case 0x0D:   /* PDSnaon                  */
            return (~((~s & d) | p)) & 0x00ffffff;
            
        case 0x0E:   /* PDSonon                  */
            return (~(~(d | s) | p)) & 0x00ffffff;
            
        case 0x0F:   /* Pn                       */
            return (~p) & 0x00ffffff;
            
        case 0x10:   /* PDSona                   */
            return (~(d | s) & p) & 0x00ffffff;
            
        case 0x11:   /* DSon        NOTSRCERASE  */
            return (~(d | s)) & 0x00ffffff;
            
        case 0x12:   /* SDPxnon                  */
            return (~(~(d ^ p) | s)) & 0x00ffffff;
            
        case 0x13:   /* SDPaon                   */
            return (~((d & p) | s)) & 0x00ffffff;
            
        case 0x14:   /* DPSxnon                  */
            return (~(~(p ^ s) | d)) & 0x00ffffff;
            
        case 0x15:   /* DPSaon                   */
            return (~((p & s) | d)) & 0x00ffffff;
            
        case 0x16:   /* PSDPSanaxx               */
            return (((~(p & s) & d) ^ s) ^ p) & 0x00ffffff;
            
        case 0x17:   /* SSPxDSxaxn               */
            return (~(((d ^ s) & (s ^ p)) ^ s)) & 0x00ffffff;
            
        case 0x18:   /* SPxPDxa                  */
            return ((s ^ p) & (p ^ d)) & 0x00ffffff;
            
        case 0x19:   /* SDPSanaxn                */
            return (~((~(p & s) & d) ^ s)) & 0x00ffffff;
            
        case 0x1A:   /* PDSPaox                  */
            return (((s & p) | d) ^ p) & 0x00ffffff;
            
        case 0x1B:   /* SDPSxaxn                 */
            return (~(((p ^ s) & d) ^ s)) & 0x00ffffff;
            
        case 0x1C:   /* PSDPaox                  */
            return (((d & p) | s) ^ p) & 0x00ffffff;
            
        case 0x1D:   /* DSPDxaxn                 */
            return (~(((p ^ d) & s) ^ d)) & 0x00ffffff;
            
        case 0x1E:   /* PDSox                    */
            return ((d | s) ^ p) & 0x00ffffff;
            
        case 0x1F:   /* PDSoan                   */
            return (~((d | s) & p)) & 0x00ffffff;
            
        case 0x20:   /* DPSnaa                   */
            return (p & ~s & d) & 0x00ffffff;
            
        case 0x21:   /* SDPxon                   */
            return (~((d ^ p) | s)) & 0x00ffffff;
            
        case 0x22:   /* DSna                     */
            return (d & ~s) & 0x00ffffff;
            
        case 0x23:   /* SPDnaon                  */
            return (~((p & ~d) | s)) & 0x00ffffff;
            
        case 0x24:   /* SPxDSxa                  */
            return ((s ^ p) & (d ^ s)) & 0x00ffffff;
            
        case 0x25:   /* PDSPanaxn                */
            return (~((~(s & p) & d) ^ p)) & 0x00ffffff;
            
        case 0x26:   /* SDPSaox                  */
            return (((p & s) | d) ^ s) & 0x00ffffff;
            
        case 0x27:   /* SDPSxnox                 */
            return ((~(p ^ s) | d) ^ s) & 0x00ffffff;
            
        case 0x28:   /* DPSxa                    */
            return ((p ^ s) & d) & 0x00ffffff;
            
        case 0x29:   /* PSDPSaoxxn               */
            return (~((((p & s) | d) ^ s) ^ p)) & 0x00ffffff;
            
        case 0x2A:   /* DPSana                   */
            return (~(p & s) & d) & 0x00ffffff;
            
        case 0x2B:   /* SSPxPDxaxn               */
            return (~(((s ^ p) & (p ^ d)) ^ s)) & 0x00ffffff;
            
        case 0x2C:   /* SPDSoax                  */
            return (((d | s) & p) ^ s) & 0x00ffffff;
            
        case 0x2D:   /* PSDnox                   */
            return ((s | ~d) ^ p) & 0x00ffffff;
            
        case 0x2E:   /* PSDPxox                  */
            return (((d ^ p) | s) ^ p) & 0x00ffffff;
            
        case 0x2F:   /* PSDnoan                  */
            return (~((s | ~d) & p)) & 0x00ffffff;
            
        case 0x30:   /* PSna                     */
            return (p & ~s) & 0x00ffffff;
            
        case 0x31:   /* SDPnaon                  */
            return (~((d & ~p) | s)) & 0x00ffffff;
            
        case 0x32:   /* SDPSoox                  */
            return ((p | s | d) ^ s) & 0x00ffffff;
            
        case 0x33:   /* Sn          NOTSRCCOPY   */
            return (~s) & 0x00ffffff;
            
        case 0x34:   /* SPDSaox                  */
            return (0x123456) & 0x00ffffff;
            
        case 0x35:   /* SPDSxnox                 */
            return (0x123456) & 0x00ffffff;
            
        case 0x36:   /* SDPox                    */
            return (0x123456) & 0x00ffffff;
            
        case 0x37:   /* SDPoan                   */
            return (0x123456) & 0x00ffffff;
            
        case 0x38:   /* PSDPoax                  */
            return (0x123456) & 0x00ffffff;
            
        case 0x39:   /* SPDnox                   */
            return (0x123456) & 0x00ffffff;
            
        case 0x3A:   /* SPDSxox                  */
            return (0x123456) & 0x00ffffff;
            
        case 0x3B:   /* SPDnoan                  */
            return (0x123456) & 0x00ffffff;
            
        case 0x3C:   /* PSx                      */
            return (0x123456) & 0x00ffffff;
            
        case 0x3D:   /* SPDSonox                 */
            return (0x123456) & 0x00ffffff;
            
        case 0x3E:   /* SPDSnaox                 */
            return (0x123456) & 0x00ffffff;
            
        case 0x3F:   /* PSan                     */
            return (0x123456) & 0x00ffffff;
            
        case 0x40:   /* PSDnaa                   */
            return (0x123456) & 0x00ffffff;
            
        case 0x41:   /* DPSxon                   */
            return (0x123456) & 0x00ffffff;
            
        case 0x42:   /* SDxPDxa                  */
            return (0x123456) & 0x00ffffff;
            
        case 0x43:   /* SPDSanaxn                */
            return (0x123456) & 0x00ffffff;
            
        case 0x44:   /* SDna        SRCERASE     */
            return (s & ~d) & 0x00ffffff;
            
        case 0x45:   /* DPSnaon                  */
            return (0x123456) & 0x00ffffff;
            
        case 0x46:   /* DSPDaox                  */
            return (0x123456) & 0x00ffffff;
            
        case 0x47:   /* PSDPxaxn                 */
            return (0x123456) & 0x00ffffff;
            
        case 0x48:   /* SDPxa                    */
            return (0x123456) & 0x00ffffff;
            
        case 0x49:   /* PDSPDaoxxn               */
            return (0x123456) & 0x00ffffff;
            
        case 0x4A:   /* DPSDoax                  */
            return (0x123456) & 0x00ffffff;
            
        case 0x4B:   /* PDSnox                   */
            return (0x123456) & 0x00ffffff;
            
        case 0x4C:   /* SDPana                   */
            return (0x123456) & 0x00ffffff;
            
        case 0x4D:   /* SSPxDSxoxn               */
            return (0x123456) & 0x00ffffff;
            
        case 0x4E:   /* PDSPxox                  */
            return (0x123456) & 0x00ffffff;
            
        case 0x4F:   /* PDSnoan                  */
            return (0x123456) & 0x00ffffff;
            
        case 0x50:   /* PDna                     */
            return (0x123456) & 0x00ffffff;
            
        case 0x51:   /* DSPnaon                  */
            return (0x123456) & 0x00ffffff;
            
        case 0x52:   /* DPSDaox                  */
            return (0x123456) & 0x00ffffff;
            
        case 0x53:   /* SPDSxaxn                 */
            return (0x123456) & 0x00ffffff;
            
        case 0x54:   /* DPSonon                  */
            return (0x123456) & 0x00ffffff;
            
        case 0x55:   /* Dn          DSTINVERT    */
            return (~d) & 0x00ffffff;
            
        case 0x56:   /* DPSox                    */
            return (0x123456) & 0x00ffffff;
            
        case 0x57:   /* DPSoan                   */
            return (0x123456) & 0x00ffffff;
            
        case 0x58:   /* PDSPoax                  */
            return (0x123456) & 0x00ffffff;
            
        case 0x59:   /* DPSnox                   */
            return (0x123456) & 0x00ffffff;
            
        case 0x5A:   /* DPx         PATINVERT    */
            return (d ^ p) & 0x00ffffff;
            
        case 0x5B:   /* DPSDonox                 */
            return (0x123456) & 0x00ffffff;
            
        case 0x5C:   /* DPSDxox                  */
            return (0x123456) & 0x00ffffff;
            
        case 0x5D:   /* DPSnoan                  */
            return (0x123456) & 0x00ffffff;
            
        case 0x5E:   /* DPSDnaox                 */
            return (0x123456) & 0x00ffffff;
            
        case 0x5F:   /* DPan                     */
            return (0x123456) & 0x00ffffff;
            
        case 0x60:   /* PDSxa                    */
            return (0x123456) & 0x00ffffff;
            
        case 0x61:   /* DSPDSaoxxn               */
            return (0x123456) & 0x00ffffff;
            
        case 0x62:   /* DSPDoax                  */
            return (0x123456) & 0x00ffffff;
            
        case 0x63:   /* SDPnox                   */
            return (0x123456) & 0x00ffffff;
            
        case 0x64:   /* SDPSoax                  */
            return (0x123456) & 0x00ffffff;
            
        case 0x65:   /* DSPnox                   */
            return (0x123456) & 0x00ffffff;
            
        case 0x66:   /* DSx         SRCINVERT    */
            return (d ^ s) & 0x00ffffff;
            
        case 0x67:   /* SDPSonox                 */
            return (0x123456) & 0x00ffffff;
            
        case 0x68:   /* DSPDSonoxxn              */
            return (0x123456) & 0x00ffffff;
            
        case 0x69:   /* PDSxxn                   */
            return (0x123456) & 0x00ffffff;
            
        case 0x6A:   /* DPSax                    */
            return (0x123456) & 0x00ffffff;
            
        case 0x6B:   /* PSDPSoaxxn               */
            return (0x123456) & 0x00ffffff;
            
        case 0x6C:   /* SDPax                    */
            return (0x123456) & 0x00ffffff;
            
        case 0x6D:   /* PDSPDoaxxn               */
            return (0x123456) & 0x00ffffff;
            
        case 0x6E:   /* SDPSnoax                 */
            return (0x123456) & 0x00ffffff;
            
        case 0x6F:   /* PDSxnan                  */
            return (0x123456) & 0x00ffffff;
            
        case 0x70:   /* PDSana                   */
            return (0x123456) & 0x00ffffff;
            
        case 0x71:   /* SSDxPDxaxn               */
            return (0x123456) & 0x00ffffff;
            
        case 0x72:   /* SDPSxox                  */
            return (0x123456) & 0x00ffffff;
            
        case 0x73:   /* SDPnoan                  */
            return (0x123456) & 0x00ffffff;
            
        case 0x74:   /* DSPDxox                  */
            return (0x123456) & 0x00ffffff;
            
        case 0x75:   /* DSPnoan                  */
            return (0x123456) & 0x00ffffff;
            
        case 0x76:   /* SDPSnaox                 */
            return (0x123456) & 0x00ffffff;
            
        case 0x77:   /* DSan                     */
            return (0x123456) & 0x00ffffff;
            
        case 0x78:   /* PDSax                    */
            return (0x123456) & 0x00ffffff;
            
        case 0x79:   /* DSPDSoaxxn               */
            return (0x123456) & 0x00ffffff;
            
        case 0x7A:   /* DPSDnoax                 */
            return (0x123456) & 0x00ffffff;
            
        case 0x7B:   /* SDPxnan                  */
            return (0x123456) & 0x00ffffff;
            
        case 0x7C:   /* SPDSnoax                 */
            return (0x123456) & 0x00ffffff;
            
        case 0x7D:   /* DPSxnan                  */
            return (0x123456) & 0x00ffffff;
            
        case 0x7E:   /* SPxDSxo                  */
            return (0x123456) & 0x00ffffff;
            
        case 0x7F:   /* DPSaan                   */
            return (0x123456) & 0x00ffffff;
            
        case 0x80:   /* DPSaa                    */
            return (0x123456) & 0x00ffffff;
            
        case 0x81:   /* SPxDSxon                 */
            return (0x123456) & 0x00ffffff;
            
        case 0x82:   /* DPSxna                   */
            return (0x123456) & 0x00ffffff;
            
        case 0x83:   /* SPDSnoaxn                */
            return (0x123456) & 0x00ffffff;
            
        case 0x84:   /* SDPxna                   */
            return (0x123456) & 0x00ffffff;
            
        case 0x85:   /* PDSPnoaxn                */
            return (~(p ^ (d & (s | ~p)))) & 0x00ffffff;
            
        case 0x86:   /* DSPDSoaxx                */
            return (0x123456) & 0x00ffffff;
            
        case 0x87:   /* PDSaxn                   */
            return (0x123456) & 0x00ffffff;
            
        case 0x88:   /* DSa         SRCAND       */
            return (d & s) & 0x00ffffff;
            
        case 0x89:   /* SDPSnaoxn                */
            return (0x123456) & 0x00ffffff;
            
        case 0x8A:   /* DSPnoa                   */
            return (0x123456) & 0x00ffffff;
            
        case 0x8B:   /* DSPDxoxn                 */
            return (0x123456) & 0x00ffffff;
            
        case 0x8C:   /* SDPnoa                   */
            return (0x123456) & 0x00ffffff;
            
        case 0x8D:   /* SDPSxoxn                 */
            return (0x123456) & 0x00ffffff;
            
        case 0x8E:   /* SSDxPDxax                */
            return (0x123456) & 0x00ffffff;
            
        case 0x8F:   /* PDSanan                  */
            return (0x123456) & 0x00ffffff;
            
        case 0x90:   /* PDSxna                   */
            return (0x123456) & 0x00ffffff;
            
        case 0x91:   /* SDPSnoaxn                */
            return (0x123456) & 0x00ffffff;
            
        case 0x92:   /* DPSDPoaxx                */
            return (0x123456) & 0x00ffffff;
            
        case 0x93:   /* SPDaxn                   */
            return (0x123456) & 0x00ffffff;
            
        case 0x94:   /* PSDPSoaxx                */
            return (0x123456) & 0x00ffffff;
            
        case 0x95:   /* DPSaxn                   */
            return (0x123456) & 0x00ffffff;
            
        case 0x96:   /* DPSxx                    */
            return (0x123456) & 0x00ffffff;
            
        case 0x97:   /* PSDPSonoxx               */
            return (0x123456) & 0x00ffffff;
            
        case 0x98:   /* SDPSonoxn                */
            return (0x123456) & 0x00ffffff;
            
        case 0x99:   /* DSxn                     */
            return (0x123456) & 0x00ffffff;
            
        case 0x9A:   /* DPSnax                   */
            return (0x123456) & 0x00ffffff;
            
        case 0x9B:   /* SDPSoaxn                 */
            return (0x123456) & 0x00ffffff;
            
        case 0x9C:   /* SPDnax                   */
            return (0x123456) & 0x00ffffff;
            
        case 0x9D:   /* DSPDoaxn                 */
            return (0x123456) & 0x00ffffff;
            
        case 0x9E:   /* DSPDSaoxx                */
            return (0x123456) & 0x00ffffff;
            
        case 0x9F:   /* PDSxan                   */
            return (0x123456) & 0x00ffffff;
            
        case 0xA0:   /* DPa                      */
            return (0x123456) & 0x00ffffff;
            
        case 0xA1:   /* PDSPnaoxn                */
            return (0x123456) & 0x00ffffff;
            
        case 0xA2:   /* DPSnoa                   */
            return (0x123456) & 0x00ffffff;
            
        case 0xA3:   /* DPSDxoxn                 */
            return (0x123456) & 0x00ffffff;
            
        case 0xA4:   /* PDSPonoxn                */
            return (0x123456) & 0x00ffffff;
            
        case 0xA5:   /* PDxn                     */
            return (0x123456) & 0x00ffffff;
            
        case 0xA6:   /* DSPnax                   */
            return (0x123456) & 0x00ffffff;
            
        case 0xA7:   /* PDSPoaxn                 */
            return (0x123456) & 0x00ffffff;
            
        case 0xA8:   /* DPSoa                    */
            return (0x123456) & 0x00ffffff;
            
        case 0xA9:   /* DPSoxn                   */
            return (0x123456) & 0x00ffffff;
            
        case 0xAA:   /* D                        */
            return (d) & 0x00ffffff;
            
        case 0xAB:   /* DPSono                   */
            return (0x123456) & 0x00ffffff;
            
        case 0xAC:   /* SPDSxax                  */
            return (s ^ (p & (d ^ s))) & 0x00ffffff;
            
        case 0xAD:   /* DPSDaoxn                 */
            return (0x123456) & 0x00ffffff;
            
        case 0xAE:   /* DSPnao                   */
            return (0x123456) & 0x00ffffff;
            
        case 0xAF:   /* DPno                     */
            return (0x123456) & 0x00ffffff;
            
        case 0xB0:   /* PDSnoa                   */
            return (0x123456) & 0x00ffffff;
            
        case 0xB1:   /* PDSPxoxn                 */
            return (0x123456) & 0x00ffffff;
            
        case 0xB2:   /* SSPxDSxox                */
            return (0x123456) & 0x00ffffff;
            
        case 0xB3:   /* SDPanan                  */
            return (0x123456) & 0x00ffffff;
            
        case 0xB4:   /* PSDnax                   */
            return (0x123456) & 0x00ffffff;
            
        case 0xB5:   /* DPSDoaxn                 */
            return (0x123456) & 0x00ffffff;
            
        case 0xB6:   /* DPSDPaoxx                */
            return (0x123456) & 0x00ffffff;
            
        case 0xB7:   /* SDPxan                   */
            return (0x123456) & 0x00ffffff;
            
        case 0xB8:   /* PSDPxax                  */
            return (0x123456) & 0x00ffffff;
            
        case 0xB9:   /* DSPDaoxn                 */
            return (0x123456) & 0x00ffffff;
            
        case 0xBA:   /* DPSnao                   */
            return (0x123456) & 0x00ffffff;
            
        case 0xBB:   /* DSno        MERGEPAINT   */
            return (d | ~s) & 0x00ffffff;
            
        case 0xBC:   /* SPDSanax                 */
            return (0x123456) & 0x00ffffff;
            
        case 0xBD:   /* SDxPDxan                 */
            return (0x123456) & 0x00ffffff;
            
        case 0xBE:   /* DPSxo                    */
            return (0x123456) & 0x00ffffff;
            
        case 0xBF:   /* DPSano                   */
            return (0x123456) & 0x00ffffff;
            
        case 0xC0:   /* PSa         MERGECOPY    */
            return (p & s) & 0x00ffffff;
            
        case 0xC1:   /* SPDSnaoxn                */
            return (0x123456) & 0x00ffffff;
            
        case 0xC2:   /* SPDSonoxn                */
            return (0x123456) & 0x00ffffff;
            
        case 0xC3:   /* PSxn                     */
            return (0x123456) & 0x00ffffff;
            
        case 0xC4:   /* SPDnoa                   */
            return (0x123456) & 0x00ffffff;
            
        case 0xC5:   /* SPDSxoxn                 */
            return (0x123456) & 0x00ffffff;
            
        case 0xC6:   /* SDPnax                   */
            return (0x123456) & 0x00ffffff;
            
        case 0xC7:   /* PSDPoaxn                 */
            return (0x123456) & 0x00ffffff;
            
        case 0xC8:   /* SDPoa                    */
            return (0x123456) & 0x00ffffff;
            
        case 0xC9:   /* SPDoxn                   */
            return (0x123456) & 0x00ffffff;
            
        case 0xCA:   /* DPSDxax                  */
            return (0x123456) & 0x00ffffff;
            
        case 0xCB:   /* SPDSaoxn                 */
            return (0x123456) & 0x00ffffff;
        
        default:    
        case 0xCC:   /* S           SRCCOPY      */
            return (s) & 0x00ffffff;
            
        case 0xCD:   /* SDPono                   */
            return (0x123456) & 0x00ffffff;
            
        case 0xCE:   /* SDPnao                   */
            return (0x123456) & 0x00ffffff;
            
        case 0xCF:   /* SPno                     */
            return (0x123456) & 0x00ffffff;
            
        case 0xD0:   /* PSDnoa                   */
            return (0x123456) & 0x00ffffff;
            
        case 0xD1:   /* PSDPxoxn                 */
            return (0x123456) & 0x00ffffff;
            
        case 0xD2:   /* PDSnax                   */
            return (0x123456) & 0x00ffffff;
            
        case 0xD3:   /* SPDSoaxn                 */
            return (0x123456) & 0x00ffffff;
            
        case 0xD4:   /* SSPxPDxax                */
            return (0x123456) & 0x00ffffff;
            
        case 0xD5:   /* DPSanan                  */
            return (0x123456) & 0x00ffffff;
            
        case 0xD6:   /* PSDPSaoxx                */
            return (0x123456) & 0x00ffffff;
            
        case 0xD7:   /* DPSxan                   */
            return (0x123456) & 0x00ffffff;
            
        case 0xD8:   /* PDSPxax                  */
            return (0x123456) & 0x00ffffff;
            
        case 0xD9:   /* SDPSaoxn                 */
            return (0x123456) & 0x00ffffff;
            
        case 0xDA:   /* DPSDanax                 */
            return (0x123456) & 0x00ffffff;
            
        case 0xDB:   /* SPxDSxan                 */
            return (0x123456) & 0x00ffffff;
            
        case 0xDC:   /* SPDnao                   */
            return (0x123456) & 0x00ffffff;
            
        case 0xDD:   /* SDno                     */
            return (0x123456) & 0x00ffffff;
            
        case 0xDE:   /* SDPxo                    */
            return (0x123456) & 0x00ffffff;
            
        case 0xDF:   /* SDPano                   */
            return (0x123456) & 0x00ffffff;
            
        case 0xE0:   /* PDSoa                    */
            return (0x123456) & 0x00ffffff;
            
        case 0xE1:   /* PDSoxn                   */
            return (0x123456) & 0x00ffffff;
            
        case 0xE2:   /* DSPDxax                  */
            return (0x123456) & 0x00ffffff;
            
        case 0xE3:   /* PSDPaoxn                 */
            return (0x123456) & 0x00ffffff;
            
        case 0xE4:   /* SDPSxax                  */
            return (0x123456) & 0x00ffffff;
            
        case 0xE5:   /* PDSPaoxn                 */
            return (0x123456) & 0x00ffffff;
            
        case 0xE6:   /* SDPSanax                 */
            return (0x123456) & 0x00ffffff;
            
        case 0xE7:   /* SPxPDxan                 */
            return (0x123456) & 0x00ffffff;
            
        case 0xE8:   /* SSPxDSxax                */
            return (0x123456) & 0x00ffffff;
            
        case 0xE9:   /* DSPDSanaxxn              */
            return (0x123456) & 0x00ffffff;
            
        case 0xEA:   /* DPSao                    */
            return (0x123456) & 0x00ffffff;
            
        case 0xEB:   /* DPSxno                   */
            return (0x123456) & 0x00ffffff;
            
        case 0xEC:   /* SDPao                    */
            return (0x123456) & 0x00ffffff;
            
        case 0xED:   /* SDPxno                   */
            return (0x123456) & 0x00ffffff;
            
        case 0xEE:   /* DSo         SRCPAINT     */
            return (d | s) & 0x00ffffff;
            
        case 0xEF:   /* SDPnoo                   */
            return (0x123456) & 0x00ffffff;
            
        case 0xF0:   /* P           PATCOPY      */
            return (p) & 0x00ffffff;
            
        case 0xF1:   /* PDSono                   */
            return (0x123456) & 0x00ffffff;
            
        case 0xF2:   /* PDSnao                   */
            return (0x123456) & 0x00ffffff;
            
        case 0xF3:   /* PSno                     */
            return (0x123456) & 0x00ffffff;
            
        case 0xF4:   /* PSDnao                   */
            return (0x123456) & 0x00ffffff;
            
        case 0xF5:   /* PDno                     */
            return (0x123456) & 0x00ffffff;
            
        case 0xF6:   /* PDSxo                    */
            return (0x123456) & 0x00ffffff;
            
        case 0xF7:   /* PDSano                   */
            return (0x123456) & 0x00ffffff;
            
        case 0xF8:   /* PDSao                    */
            return (0x123456) & 0x00ffffff;
            
        case 0xF9:   /* PDSxno                   */
            return (0x123456) & 0x00ffffff;
            
        case 0xFA:   /* DPo                      */
            return (0x123456) & 0x00ffffff;
            
        case 0xFB:   /* DPSnoo      PATPAINT     */
            return (p | ~s | d) & 0x00ffffff;
            
        case 0xFC:   /* PSo                      */
            return (0x123456) & 0x00ffffff;
            
        case 0xFD:   /* PSDnoo                   */
            return (0x123456) & 0x00ffffff;
            
        case 0xFE:   /* DPSoo                    */
            return (0x123456) & 0x00ffffff;
            
        case 0xFF:   /* 1           WHITENESS    */
            return 0x00ffffff;
    
    } /* switch */
}
