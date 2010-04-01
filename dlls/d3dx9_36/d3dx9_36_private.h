/*
 * Copyright (C) 2002 Raphael Junqueira
 * Copyright (C) 2008 David Adam
 * Copyright (C) 2008 Tony Wasserka
 * Copyright (C) 2008 Stefan Dösinger
 * Copyright (C) 2009 Matteo Bruni
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 *
 */

#ifndef __WINE_D3DX9_36_PRIVATE_H
#define __WINE_D3DX9_36_PRIVATE_H

#include <stdarg.h>

#define COBJMACROS
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "d3dx9.h"

/* for internal use */
typedef enum _FormatType {
    FORMAT_ARGB,   /* unsigned */
    FORMAT_UNKNOWN
} FormatType;

typedef struct _PixelFormatDesc {
    D3DFORMAT format;
    BYTE bits[4];
    BYTE shift[4];
    UINT bytes_per_pixel;
    FormatType type;
} PixelFormatDesc;

HRESULT map_view_of_file(LPCWSTR filename, LPVOID *buffer, DWORD *length);
HRESULT load_resource_into_memory(HMODULE module, HRSRC resinfo, LPVOID *buffer, DWORD *length);

const PixelFormatDesc *get_format_info(D3DFORMAT format);


extern const ID3DXBufferVtbl D3DXBuffer_Vtbl;

/* ID3DXBUFFER */
typedef struct ID3DXBufferImpl
{
    /* IUnknown fields */
    const ID3DXBufferVtbl *lpVtbl;
    LONG           ref;

    /* ID3DXBuffer fields */
    DWORD         *buffer;
    DWORD          bufferSize;
} ID3DXBufferImpl;


/* ID3DXFont */
typedef struct ID3DXFontImpl
{
    /* IUnknown fields */
    const ID3DXFontVtbl *lpVtbl;
    LONG ref;

    /* ID3DXFont fields */
    IDirect3DDevice9 *device;
    D3DXFONT_DESCW desc;

    HDC hdc;
    HFONT hfont;
} ID3DXFontImpl;

/* ID3DXMatrixStack */
typedef struct ID3DXMatrixStackImpl
{
  /* IUnknown fields */
  const ID3DXMatrixStackVtbl *lpVtbl;
  LONG                   ref;

  /* ID3DXMatrixStack fields */
  unsigned int current;
  unsigned int stack_size;
  D3DXMATRIX *stack;
} ID3DXMatrixStackImpl;

/*ID3DXSprite */
typedef struct _SPRITE {
    LPDIRECT3DTEXTURE9 texture;
    UINT texw, texh;
    RECT rect;
    D3DXVECTOR3 center;
    D3DXVECTOR3 pos;
    D3DCOLOR color;
} SPRITE;

typedef struct ID3DXSpriteImpl
{
    /* IUnknown fields */
    const ID3DXSpriteVtbl *lpVtbl;
    LONG ref;

    /* ID3DXSprite fields */
    IDirect3DDevice9 *device;
    IDirect3DVertexDeclaration9 *vdecl;
    IDirect3DStateBlock9 *stateblock;
    D3DXMATRIX transform;
    D3DXMATRIX view;
    DWORD flags;
    BOOL ready;

    /* Store the relevant caps to prevent multiple GetDeviceCaps calls */
    DWORD texfilter_caps;
    DWORD maxanisotropy;
    DWORD alphacmp_caps;

    SPRITE *sprites;
    int sprite_count;      /* number of sprites to be drawn */
    int allocated_sprites; /* number of (pre-)allocated sprites */
} ID3DXSpriteImpl;

/* Shader assembler definitions */
typedef enum _shader_type {
    ST_VERTEX,
    ST_PIXEL,
} shader_type;

typedef enum BWRITER_COMPARISON_TYPE {
    BWRITER_COMPARISON_NONE = 0,
} BWRITER_COMPARISON_TYPE;

struct shader_reg {
    DWORD                   type;
    DWORD                   regnum;
    struct shader_reg       *rel_reg;
    DWORD                   srcmod;
    union {
        DWORD               swizzle;
        DWORD               writemask;
    };
};

struct instruction {
    DWORD                   opcode;
    DWORD                   dstmod;
    DWORD                   shift;
    BWRITER_COMPARISON_TYPE comptype;
    BOOL                    has_dst;
    struct shader_reg       dst;
    struct shader_reg       *src;
    unsigned int            num_srcs; /* For freeing the rel_regs */
    BOOL                    has_predicate;
    struct shader_reg       predicate;
};

struct declaration {
    DWORD                   usage, usage_idx;
    DWORD                   regnum;
    DWORD                   writemask;
};

struct samplerdecl {
    DWORD                   type;
    DWORD                   regnum;
    unsigned int            line_no; /* for error messages */
};

#define INSTRARRAY_INITIAL_SIZE 8
struct bwriter_shader {
    shader_type             type;

    /* Shader version selected */
    DWORD                   version;

    /* Local constants. Every constant that is not defined below is loaded from
     * the global constant set at shader runtime
     */
    struct constant         **constF;
    struct constant         **constI;
    struct constant         **constB;
    unsigned int            num_cf, num_ci, num_cb;

    /* Declared input and output varyings */
    struct declaration      *inputs, *outputs;
    unsigned int            num_inputs, num_outputs;
    struct samplerdecl      *samplers;
    unsigned int            num_samplers;

    /* Are special pixel shader 3.0 registers declared? */
    BOOL                    vPos, vFace;

    /* Array of shader instructions - The shader code itself */
    struct instruction      **instr;
    unsigned int            num_instrs, instr_alloc_size;
};

static inline LPVOID asm_alloc(SIZE_T size) {
    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size);
}

static inline LPVOID asm_realloc(LPVOID ptr, SIZE_T size) {
    return HeapReAlloc(GetProcessHeap(), 0, ptr, size);
}

static inline BOOL asm_free(LPVOID ptr) {
    return HeapFree(GetProcessHeap(), 0, ptr);
}

struct asm_parser;

/* This structure is only used in asmshader.y, but since the .l file accesses the semantic types
 * too it has to know it as well
 */
struct rel_reg {
    BOOL            has_rel_reg;
    DWORD           type;
    DWORD           additional_offset;
    DWORD           rel_regnum;
    DWORD           swizzle;
};

#define MAX_SRC_REGS 4

struct src_regs {
    struct shader_reg reg[MAX_SRC_REGS];
    unsigned int      count;
};

struct asmparser_backend {
    void (*dstreg)(struct asm_parser *This, struct instruction *instr,
                   const struct shader_reg *dst);
    void (*srcreg)(struct asm_parser *This, struct instruction *instr, int num,
                   const struct shader_reg *src);

    void (*predicate)(struct asm_parser *This,
                      const struct shader_reg *predicate);
    void (*coissue)(struct asm_parser *This);

    void (*end)(struct asm_parser *This);

    void (*instr)(struct asm_parser *This, DWORD opcode, DWORD mod, DWORD shift,
                  BWRITER_COMPARISON_TYPE comp, const struct shader_reg *dst,
                  const struct src_regs *srcs, int expectednsrcs);
};

struct instruction *alloc_instr(unsigned int srcs);
BOOL add_instruction(struct bwriter_shader *shader, struct instruction *instr);

#define MESSAGEBUFFER_INITIAL_SIZE 256

struct asm_parser {
    /* The function table of the parser implementation */
    const struct asmparser_backend *funcs;

    /* Private data follows */
    struct bwriter_shader *shader;
    unsigned int m3x3pad_count;

    enum parse_status {
        PARSE_SUCCESS = 0,
        PARSE_WARN = 1,
        PARSE_ERR = 2
    } status;
    char *messages;
    unsigned int messagesize;
    unsigned int messagecapacity;
    unsigned int line_no;
};

extern struct asm_parser asm_ctx;

void create_vs30_parser(struct asm_parser *ret);

struct bwriter_shader *parse_asm_shader(char **messages);

#ifdef __GNUC__
#define PRINTF_ATTR(fmt,args) __attribute__((format (printf,fmt,args)))
#else
#define PRINTF_ATTR(fmt,args)
#endif

void asmparser_message(struct asm_parser *ctx, const char *fmt, ...) PRINTF_ATTR(2,3);
void set_parse_status(struct asm_parser *ctx, enum parse_status status);

/* A reasonable value as initial size */
#define BYTECODEBUFFER_INITIAL_SIZE 32
struct bytecode_buffer {
    DWORD *data;
    DWORD size;
    DWORD alloc_size;
    /* For tracking rare out of memory situations without passing
     * return values around everywhere
     */
    HRESULT state;
};

struct bc_writer; /* Predeclaration for use in vtable parameters */

typedef void (*instr_writer)(struct bc_writer *This,
                             const struct instruction *instr,
                             struct bytecode_buffer *buffer);

struct bytecode_backend {
    void (*header)(struct bc_writer *This, const struct bwriter_shader *shader,
		   struct bytecode_buffer *buffer);
    void (*end)(struct bc_writer *This, const struct bwriter_shader *shader,
                struct bytecode_buffer *buffer);
    void (*srcreg)(struct bc_writer *This, const struct shader_reg *reg,
                   struct bytecode_buffer *buffer);
    void (*dstreg)(struct bc_writer *This, const struct shader_reg *reg,
                   struct bytecode_buffer *buffer, DWORD shift, DWORD mod);
    void (*opcode)(struct bc_writer *This, const struct instruction *instr,
                   DWORD token, struct bytecode_buffer *buffer);

    const struct instr_handler_table {
        DWORD opcode;
        instr_writer func;
    } *instructions;
};

/* Bytecode writing stuff */
struct bc_writer {
    const struct bytecode_backend *funcs;

    /* Avoid result checking */
    HRESULT                       state;

    DWORD                         version;
};

/* Debug utility routines */
const char *debug_print_dstreg(const struct shader_reg *reg, shader_type st);
const char *debug_print_srcreg(const struct shader_reg *reg, shader_type st);
const char *debug_print_opcode(DWORD opcode);

/* Utilities for internal->d3d constant mapping */
DWORD d3d9_swizzle(DWORD bwriter_swizzle);
DWORD d3d9_writemask(DWORD bwriter_writemask);
DWORD d3d9_register(DWORD bwriter_register);
DWORD d3d9_opcode(DWORD bwriter_opcode);

/*
  Enumerations and defines used in the bytecode writer
  intermediate representation
*/
typedef enum _BWRITERSHADER_INSTRUCTION_OPCODE_TYPE {
    BWRITERSIO_MOV = 1,

    BWRITERSIO_COMMENT = 0xfffe,
    BWRITERSIO_END = 0Xffff,
} BWRITERSHADER_INSTRUCTION_OPCODE_TYPE;

typedef enum _BWRITERSHADER_PARAM_REGISTER_TYPE {
    BWRITERSPR_TEMP = 0,
    BWRITERSPR_CONST = 2,
} BWRITERSHADER_PARAM_REGISTER_TYPE;

#define BWRITERSP_WRITEMASK_0   0x1 /* .x r */
#define BWRITERSP_WRITEMASK_1   0x2 /* .y g */
#define BWRITERSP_WRITEMASK_2   0x4 /* .z b */
#define BWRITERSP_WRITEMASK_3   0x8 /* .w a */
#define BWRITERSP_WRITEMASK_ALL 0xf /* all */

typedef enum _BWRITERSHADER_PARAM_SRCMOD_TYPE {
    BWRITERSPSM_NONE = 0,
} BWRITERSHADER_PARAM_SRCMOD_TYPE;

#define BWRITER_SM1_VS  0xfffe
#define BWRITER_SM1_PS  0xffff

#define BWRITERPS_VERSION(major, minor) ((BWRITER_SM1_PS << 16) | ((major) << 8) | (minor))
#define BWRITERVS_VERSION(major, minor) ((BWRITER_SM1_VS << 16) | ((major) << 8) | (minor))

#define BWRITERVS_SWIZZLE_SHIFT      16
#define BWRITERVS_SWIZZLE_MASK       (0xFF << BWRITERVS_SWIZZLE_SHIFT)

#define BWRITERVS_X_X       (0 << BWRITERVS_SWIZZLE_SHIFT)
#define BWRITERVS_X_Y       (1 << BWRITERVS_SWIZZLE_SHIFT)
#define BWRITERVS_X_Z       (2 << BWRITERVS_SWIZZLE_SHIFT)
#define BWRITERVS_X_W       (3 << BWRITERVS_SWIZZLE_SHIFT)

#define BWRITERVS_Y_X       (0 << (BWRITERVS_SWIZZLE_SHIFT + 2))
#define BWRITERVS_Y_Y       (1 << (BWRITERVS_SWIZZLE_SHIFT + 2))
#define BWRITERVS_Y_Z       (2 << (BWRITERVS_SWIZZLE_SHIFT + 2))
#define BWRITERVS_Y_W       (3 << (BWRITERVS_SWIZZLE_SHIFT + 2))

#define BWRITERVS_Z_X       (0 << (BWRITERVS_SWIZZLE_SHIFT + 4))
#define BWRITERVS_Z_Y       (1 << (BWRITERVS_SWIZZLE_SHIFT + 4))
#define BWRITERVS_Z_Z       (2 << (BWRITERVS_SWIZZLE_SHIFT + 4))
#define BWRITERVS_Z_W       (3 << (BWRITERVS_SWIZZLE_SHIFT + 4))

#define BWRITERVS_W_X       (0 << (BWRITERVS_SWIZZLE_SHIFT + 6))
#define BWRITERVS_W_Y       (1 << (BWRITERVS_SWIZZLE_SHIFT + 6))
#define BWRITERVS_W_Z       (2 << (BWRITERVS_SWIZZLE_SHIFT + 6))
#define BWRITERVS_W_W       (3 << (BWRITERVS_SWIZZLE_SHIFT + 6))

#define BWRITERVS_NOSWIZZLE (BWRITERVS_X_X | BWRITERVS_Y_Y | BWRITERVS_Z_Z | BWRITERVS_W_W)

struct bwriter_shader *SlAssembleShader(const char *text, char **messages);
DWORD SlWriteBytecode(const struct bwriter_shader *shader, int dxversion, DWORD **result);
void SlDeleteShader(struct bwriter_shader *shader);

#endif /* __WINE_D3DX9_36_PRIVATE_H */
