/*
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
 */
#define COBJMACROS
#include "wine/test.h"

#include <d3dx9.h>

#include "resources.h"

struct shader_test {
    const char *text;
    const DWORD bytes[128];
};

static HRESULT create_file(const char *filename, const char *data, const unsigned int size)
{
    DWORD received;
    HANDLE hfile;

    hfile = CreateFileA(filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if(hfile == INVALID_HANDLE_VALUE) return HRESULT_FROM_WIN32(GetLastError());

    if(WriteFile(hfile, data, size, &received, NULL))
    {
        CloseHandle(hfile);
        return D3D_OK;
    }

    CloseHandle(hfile);
    return D3DERR_INVALIDCALL;
}

static void dump_shader(DWORD *shader) {
    unsigned int i = 0, j = 0;
    do {
        trace("0x%08x ", shader[i]);
        j++;
        i++;
        if(j == 6) trace("\n");
    } while(shader[i - 1] != D3DSIO_END);
    if(j != 6) trace("\n");
}

static void exec_tests(const char *name, struct shader_test tests[], unsigned int count) {
    HRESULT hr;
    DWORD *res;
    unsigned int i, j;
    BOOL diff;
    LPD3DXBUFFER shader, messages;

    for(i = 0; i < count; i++) {
        /* D3DXAssembleShader sets messages to 0 if there aren't error messages */
        messages = NULL;
        hr = D3DXAssembleShader(tests[i].text, strlen(tests[i].text),
                                NULL, NULL, D3DXSHADER_SKIPVALIDATION,
                                &shader, &messages);
        ok(hr == D3D_OK, "Test %s, shader %d: D3DXAssembleShader failed with error 0x%x - %d\n", name, i, hr, hr & 0x0000FFFF);
        if(messages) {
            trace("D3DXAssembleShader messages:\n%s", (char *)ID3DXBuffer_GetBufferPointer(messages));
            ID3DXBuffer_Release(messages);
        }
        if(FAILED(hr)) continue;

        j = 0;
        diff = FALSE;
        res = ID3DXBuffer_GetBufferPointer(shader);
        while(res[j] != D3DSIO_END && tests[i].bytes[j] != D3DSIO_END) {
            if(res[j] != tests[i].bytes[j]) diff = TRUE;
            j++;
        };
        /* Both must have an end token */
        if(res[j] != tests[i].bytes[j]) diff = TRUE;

        if(diff) {
            ok(FALSE, "Test %s, shader %d: Generated code differs\n", name, i);
            dump_shader(res);
        }
        ID3DXBuffer_Release(shader);
    }
}

static void preproc_test(void) {
    struct shader_test tests[] = {
        {   /* shader 0 */
            "vs.1.1\r\n"
            "//some comments\r\n"
            "//other comments\n"
            "; yet another comment\r\n"
            "add r0, r0, r1\n",
            {0xfffe0101, 0x00000002, 0x800f0000, 0x80e40000, 0x80e40001, 0x0000ffff}
        },
        {   /* shader 1 */
            "#line 1 \"vertex.vsh\"\n"
            "vs.1.1\n",
            {0xfffe0101, 0x0000ffff}
        },
        {   /* shader 2 */
            "#define REG 1 + 2 +\\\n"
            "3 + 4\n"
            "vs.1.1\n"
            "mov r0, c0[ REG ]\n",
            {0xfffe0101, 0x00000001, 0x800f0000, 0xa0e4000a, 0x0000ffff}
        },
    };

    exec_tests("preproc", tests, sizeof(tests) / sizeof(tests[0]));
}

static void ps_1_1_test(void) {
    struct shader_test tests[] = {
        {   /* shader 0 */
            "ps.1.1\r\n"
            "tex t0\r\n"
            "add r0.rgb, r0, r1\r\n"
            "+mov r0.a, t0\r\n",
            {0xffff0101, 0x00000042, 0xb00f0000, 0x00000002, 0x80070000, 0x80e40000,
             0x80e40001, 0x40000001, 0x80080000, 0xb0e40000, 0x0000ffff}
        },
    };

    exec_tests("ps_1_1", tests, sizeof(tests) / sizeof(tests[0]));
}

static void vs_1_1_test(void) {
    struct shader_test tests[] = {
        /* Basic instruction tests */
        {   /* shader 0 */
            "vs_1_1\n"
            "add r0, r1, r2\n",
            {0xfffe0101, 0x00000002, 0x800f0000, 0x80e40001, 0x80e40002, 0x0000ffff}
        },
        {   /* shader 1 */
            "vs_1_1\n"
            "nop\n",
            {0xfffe0101, 0x00000000, 0x0000ffff}
        },
        /* Output register tests */
        {   /* shader 2 */
            "vs_1_1\n"
            "mov oPos, c0\n",
            {0xfffe0101, 0x00000001, 0xc00f0000, 0xa0e40000, 0x0000ffff}
        },
        {   /* shader 3 */
            "vs_1_1\n"
            "mov oT0, c0\n",
            {0xfffe0101, 0x00000001, 0xe00f0000, 0xa0e40000, 0x0000ffff}
        },
        {   /* shader 4 */
            "vs_1_1\n"
            "mov oT5, c0\n",
            {0xfffe0101, 0x00000001, 0xe00f0005, 0xa0e40000, 0x0000ffff}
        },
        {   /* shader 5 */
            "vs_1_1\n"
            "mov oD0, c0\n",
            {0xfffe0101, 0x00000001, 0xd00f0000, 0xa0e40000, 0x0000ffff}
        },
        {   /* shader 6 */
            "vs_1_1\n"
            "mov oD1, c0\n",
            {0xfffe0101, 0x00000001, 0xd00f0001, 0xa0e40000, 0x0000ffff}
        },
        {   /* shader 7 */
            "vs_1_1\n"
            "mov oFog, c0.x\n",
            {0xfffe0101, 0x00000001, 0xc00f0001, 0xa0000000, 0x0000ffff}
        },
        {   /* shader 8 */
            "vs_1_1\n"
            "mov oPts, c0.x\n",
            {0xfffe0101, 0x00000001, 0xc00f0002, 0xa0000000, 0x0000ffff}
        },
        /* A bunch of tests for declarations */
        {   /* shader 9 */
            "vs_1_1\n"
            "dcl_position0 v0",
            {0xfffe0101, 0x0000001f, 0x80000000, 0x900f0000, 0x0000ffff}
        },
        {   /* shader 10 */
            "vs_1_1\n"
            "dcl_position v1",
            {0xfffe0101, 0x0000001f, 0x80000000, 0x900f0001, 0x0000ffff}
        },
        {   /* shader 11 */
            "vs_1_1\n"
            "dcl_normal12 v15",
            {0xfffe0101, 0x0000001f, 0x800c0003, 0x900f000f, 0x0000ffff}
        },
        {   /* shader 12 */
            "vs_1_1\n"
            "add r0, v0, v1\n",
            {0xfffe0101, 0x00000002, 0x800f0000, 0x90e40000, 0x90e40001, 0x0000ffff}
        },
        {   /* shader 13 */
            "vs_1_1\n"
            "def c12, 0, -1, -0.5, 1024\n",
            {0xfffe0101, 0x00000051, 0xa00f000c, 0x00000000, 0xbf800000, 0xbf000000,
             0x44800000, 0x0000ffff}
        },
        {   /* shader 14: writemasks, swizzles */
            "vs_1_1\n"
            "dp4 r0.xw, r1.wzyx, r2.xxww\n",
            {0xfffe0101, 0x00000009, 0x80090000, 0x801b0001, 0x80f00002, 0x0000ffff}
        },
        {   /* shader 15: negation input modifier. Other modifiers not supprted in vs_1_1 */
            "vs_1_1\n"
            "add r0, -r0.x, -r1\n",
            {0xfffe0101, 0x00000002, 0x800f0000, 0x81000000, 0x81e40001, 0x0000ffff}
        },
        {   /* shader 16: relative addressing */
            "vs_1_1\n"
            "mov r0, c0[a0.x]\n",
            {0xfffe0101, 0x00000001, 0x800f0000, 0xa0e42000, 0x0000ffff}
        },
        {   /* shader 17: relative addressing */
            "vs_1_1\n"
            "mov r0, c1[a0.x + 2]\n",
            {0xfffe0101, 0x00000001, 0x800f0000, 0xa0e42003, 0x0000ffff}
        },
        {   /* shader 18 */
            "vs_1_1\n"
            "def c0, 1.0f, 1.0f, 1.0f, 0.5f\n",
            {0xfffe0101, 0x00000051, 0xa00f0000, 0x3f800000, 0x3f800000, 0x3f800000,
             0x3f000000, 0x0000ffff}
        },
        /* Other relative addressing tests */
        {   /* shader 19 */
            "vs_1_1\n"
            "mov r0, c[ a0.x + 12 ]\n",
            {0xfffe0101, 0x00000001, 0x800f0000, 0xa0e4200c, 0x0000ffff}
        },
        {   /* shader 20 */
            "vs_1_1\n"
            "mov r0, c[ 2 + a0.x ]\n",
            {0xfffe0101, 0x00000001, 0x800f0000, 0xa0e42002, 0x0000ffff}
        },
        {   /* shader 21 */
            "vs_1_1\n"
            "mov r0, c[ 2 + a0.x + 12 ]\n",
            {0xfffe0101, 0x00000001, 0x800f0000, 0xa0e4200e, 0x0000ffff}
        },
        {   /* shader 22 */
            "vs_1_1\n"
            "mov r0, c[ 2 + 10 + 12 ]\n",
            {0xfffe0101, 0x00000001, 0x800f0000, 0xa0e40018, 0x0000ffff}
        },
        {   /* shader 23 */
            "vs_1_1\n"
            "mov r0, c4[ 2 ]\n",
            {0xfffe0101, 0x00000001, 0x800f0000, 0xa0e40006, 0x0000ffff}
        },
        {   /* shader 24 */
            "vs_1_1\n"
            "rcp r0, v0.x\n",
            {0xfffe0101, 0x00000006, 0x800f0000, 0x90000000, 0x0000ffff}
        },
        {   /* shader 25 */
            "vs_1_1\n"
            "rsq r0, v0.x\n",
            {0xfffe0101, 0x00000007, 0x800f0000, 0x90000000, 0x0000ffff}
        },
    };

    exec_tests("vs_1_1", tests, sizeof(tests) / sizeof(tests[0]));
}

static void ps_1_3_test(void) {
    struct shader_test tests[] = {
        /* Basic instruction tests */
        {   /* shader 0 */
            "ps_1_3\n"
            "mov r0, r1\n",
            {0xffff0103, 0x00000001, 0x800f0000, 0x80e40001, 0x0000ffff}
        },
        {   /* shader 1 */
            "ps_1_3\n"
            "add r0, r1, r0\n",
            {0xffff0103, 0x00000002, 0x800f0000, 0x80e40001, 0x80e40000, 0x0000ffff}
        },
        /* Color interpolator tests */
        {   /* shader 2 */
            "ps_1_3\n"
            "mov r0, v0\n",
            {0xffff0103, 0x00000001, 0x800f0000, 0x90e40000, 0x0000ffff}
        },
        {   /* shader 3 */
            "ps_1_3\n"
            "mov r0, v1\n",
            {0xffff0103, 0x00000001, 0x800f0000, 0x90e40001, 0x0000ffff}
        },
        /* Texture sampling instructions */
        {   /* shader 4 */
            "ps_1_3\n"
            "tex t0\n",
            {0xffff0103, 0x00000042, 0xb00f0000, 0x0000ffff}
        },
        {   /* shader 5 */
            "ps_1_3\n"
            "tex t0\n"
            "texreg2ar t1, t0\n",
            {0xffff0103, 0x00000042, 0xb00f0000, 0x00000045, 0xb00f0001, 0xb0e40000,
             0x0000ffff}
        },
        {   /* shader 6 */
            "ps_1_3\n"
            "tex t0\n"
            "texreg2gb t1, t0\n",
            {0xffff0103, 0x00000042, 0xb00f0000, 0x00000046, 0xb00f0001, 0xb0e40000,
             0x0000ffff}
        },
        {   /* shader 7 */
            "ps_1_3\n"
            "tex t0\n"
            "texreg2rgb t1, t0\n",
            {0xffff0103, 0x00000042, 0xb00f0000, 0x00000052, 0xb00f0001, 0xb0e40000,
             0x0000ffff}
        },
        {   /* shader 8 */
            "ps_1_3\n"
            "cnd r0, r1, r0, v0\n",
            {0xffff0103, 0x00000050, 0x800f0000, 0x80e40001, 0x80e40000, 0x90e40000,
             0x0000ffff}
        },
        {   /* shader 9 */
            "ps_1_3\n"
            "cmp r0, r1, r0, v0\n",
            {0xffff0103, 0x00000058, 0x800f0000, 0x80e40001, 0x80e40000, 0x90e40000,
             0x0000ffff}
        },
        {   /* shader 10 */
            "ps_1_3\n"
            "texkill t0\n",
            {0xffff0103, 0x00000041, 0xb00f0000, 0x0000ffff}
        },
        {   /* shader 11 */
            "ps_1_3\n"
            "tex t0\n"
            "texm3x2pad t1, t0\n"
            "texm3x2tex t2, t0\n",
            {0xffff0103, 0x00000042, 0xb00f0000, 0x00000047, 0xb00f0001, 0xb0e40000,
             0x00000048, 0xb00f0002, 0xb0e40000, 0x0000ffff}
        },
        {   /* shader 12 */
            "ps_1_3\n"
            "tex t0\n"
            "texm3x2pad t1, t0\n"
            "texm3x2depth t2, t0\n",
            {0xffff0103, 0x00000042, 0xb00f0000, 0x00000047, 0xb00f0001, 0xb0e40000,
             0x00000054, 0xb00f0002, 0xb0e40000, 0x0000ffff}
        },
        {   /* shader 13 */
            "ps_1_3\n"
            "tex t0\n"
            "texbem t1, t0\n",
            {0xffff0103, 0x00000042, 0xb00f0000, 0x00000043, 0xb00f0001, 0xb0e40000,
             0x0000ffff}
        },
        {   /* shader 14 */
            "ps_1_3\n"
            "tex t0\n"
            "texbeml t1, t0\n",
            {0xffff0103, 0x00000042, 0xb00f0000, 0x00000044, 0xb00f0001, 0xb0e40000,
             0x0000ffff}
        },
        {   /* shader 15 */
            "ps_1_3\n"
            "tex t0\n"
            "texdp3tex t1, t0\n",
            {0xffff0103, 0x00000042, 0xb00f0000, 0x00000053, 0xb00f0001, 0xb0e40000,
             0x0000ffff}
        },
        {   /* shader 16 */
            "ps_1_3\n"
            "tex t0\n"
            "texdp3 t1, t0\n",
            {0xffff0103, 0x00000042, 0xb00f0000, 0x00000055, 0xb00f0001, 0xb0e40000,
             0x0000ffff}
        },
        {   /* shader 17 */
            "ps_1_3\n"
            "tex t0\n"
            "texm3x3pad t1, t0\n"
            "texm3x3pad t2, t0\n"
            "texm3x3tex t3, t0\n",
            {0xffff0103, 0x00000042, 0xb00f0000, 0x00000049, 0xb00f0001, 0xb0e40000,
             0x00000049, 0xb00f0002, 0xb0e40000, 0x0000004a, 0xb00f0003, 0xb0e40000,
             0x0000ffff}
        },
        {   /* shader 18 */
            "ps_1_3\n"
            "tex t0\n"
            "texm3x3pad t1, t0\n"
            "texm3x3pad t2, t0\n"
            "texm3x3 t3, t0\n",
            {0xffff0103, 0x00000042, 0xb00f0000, 0x00000049, 0xb00f0001, 0xb0e40000,
             0x00000049, 0xb00f0002, 0xb0e40000, 0x00000056, 0xb00f0003, 0xb0e40000,
             0x0000ffff}
        },
        {   /* shader 19 */
            "ps_1_3\n"
            "tex t0\n"
            "texm3x3pad t1, t0\n"
            "texm3x3pad t2, t0\n"
            "texm3x3spec t3, t0, c0\n",
            {0xffff0103, 0x00000042, 0xb00f0000, 0x00000049, 0xb00f0001, 0xb0e40000,
             0x00000049, 0xb00f0002, 0xb0e40000, 0x0000004c, 0xb00f0003, 0xb0e40000,
             0xa0e40000, 0x0000ffff}
        },
        {   /* shader 20 */
            "ps_1_3\n"
            "tex t0\n"
            "texm3x3pad t1, t0\n"
            "texm3x3pad t2, t0\n"
            "texm3x3vspec t3, t0\n",
            {0xffff0103, 0x00000042, 0xb00f0000, 0x00000049, 0xb00f0001, 0xb0e40000,
             0x00000049, 0xb00f0002, 0xb0e40000, 0x0000004d, 0xb00f0003, 0xb0e40000,
             0x0000ffff}
        },
        {   /* shader 21 */
            "ps_1_3\n"
            "texcoord t0\n",
            {0xffff0103, 0x00000040, 0xb00f0000, 0x0000ffff}
        },
        /* Modifiers, shifts */
        {   /* shader 22 */
            "ps_1_3\n"
            "mov_x2_sat r0, 1 - r1\n",
            {0xffff0103, 0x00000001, 0x811f0000, 0x86e40001, 0x0000ffff}
        },
        {   /* shader 23 */
            "ps_1_3\n"
            "mov_d8 r0, -r1\n",
            {0xffff0103, 0x00000001, 0x8d0f0000, 0x81e40001, 0x0000ffff}
        },
        {   /* shader 24 */
            "ps_1_3\n"
            "mov_sat r0, r1_bx2\n",
            {0xffff0103, 0x00000001, 0x801f0000, 0x84e40001, 0x0000ffff}
        },
        {   /* shader 25 */
            "ps_1_3\n"
            "mov_sat r0, r1_bias\n",
            {0xffff0103, 0x00000001, 0x801f0000, 0x82e40001, 0x0000ffff}
        },
        {   /* shader 26 */
            "ps_1_3\n"
            "mov_sat r0, -r1_bias\n",
            {0xffff0103, 0x00000001, 0x801f0000, 0x83e40001, 0x0000ffff}
        },
        {   /* shader 27 */
            "ps_1_3\n"
            "mov_sat r0, -r1_bx2\n",
            {0xffff0103, 0x00000001, 0x801f0000, 0x85e40001, 0x0000ffff}
        },
        {   /* shader 28 */
            "ps_1_3\n"
            "mov_sat r0, -r1_x2\n",
            {0xffff0103, 0x00000001, 0x801f0000, 0x88e40001, 0x0000ffff}
        },
        {   /* shader 29 */
            "ps_1_3\n"
            "mov_x4_sat r0.a, -r1_bx2.a\n",
            {0xffff0103, 0x00000001, 0x82180000, 0x85ff0001, 0x0000ffff}
        },
    };

    exec_tests("ps_1_3", tests, sizeof(tests) / sizeof(tests[0]));
}

static void ps_1_4_test(void) {
    struct shader_test tests[] = {
        /* Basic instruction tests */
        {   /* shader 0 */
            "ps_1_4\n"
            "mov r0, r1\n",
            {0xffff0104, 0x00000001, 0x800f0000, 0x80e40001, 0x0000ffff}
        },
        {   /* shader 1 */
            "ps_1_4\n"
            "mov r0, r5\n",
            {0xffff0104, 0x00000001, 0x800f0000, 0x80e40005, 0x0000ffff}
        },
        {   /* shader 2 */
            "ps_1_4\n"
            "mov r0, c7\n",
            {0xffff0104, 0x00000001, 0x800f0000, 0xa0e40007, 0x0000ffff}
        },
        {   /* shader 3 */
            "ps_1_4\n"
            "mov r0, v1\n",
            {0xffff0104, 0x00000001, 0x800f0000, 0x90e40001, 0x0000ffff}
        },
        {   /* shader 4 */
            "ps_1_4\n"
            "phase\n",
            {0xffff0104, 0x0000fffd, 0x0000ffff}
        },
        {   /* shader 5 */
            "ps_1_4\n"
            "texcrd r0, t0\n",
            {0xffff0104, 0x00000040, 0x800f0000, 0xb0e40000, 0x0000ffff}
        },
        {   /* shader 6 */
            "ps_1_4\n"
            "texcrd r4, t3\n",
            {0xffff0104, 0x00000040, 0x800f0004, 0xb0e40003, 0x0000ffff}
        },
        {   /* shader 7 */
            "ps_1_4\n"
            "texcrd_sat r4, t3\n",
            {0xffff0104, 0x00000040, 0x801f0004, 0xb0e40003, 0x0000ffff}
        },
        {   /* shader 8 */
            "ps_1_4\n"
            "texld r0, t0\n",
            {0xffff0104, 0x00000042, 0x800f0000, 0xb0e40000, 0x0000ffff}
        },
        {   /* shader 9 */
            "ps_1_4\n"
            "texld r1, t4\n",
            {0xffff0104, 0x00000042, 0x800f0001, 0xb0e40004, 0x0000ffff}
        },
        {   /* shader 10 */
            "ps_1_4\n"
            "texld r5, r0\n",
            {0xffff0104, 0x00000042, 0x800f0005, 0x80e40000, 0x0000ffff}
        },
        {   /* shader 11 */
            "ps_1_4\n"
            "texld r5, c0\n", /* Assembly succeeds, validation fails */
            {0xffff0104, 0x00000042, 0x800f0005, 0xa0e40000, 0x0000ffff}
        },
        {   /* shader 12 */
            "ps_1_4\n"
            "texld r5, r2_dz\n",
            {0xffff0104, 0x00000042, 0x800f0005, 0x89e40002, 0x0000ffff}
        },
        {   /* shader 13 */
            "ps_1_4\n"
            "bem r1.rg, c0, r0\n",
            {0xffff0104, 0x00000059, 0x80030001, 0xa0e40000, 0x80e40000, 0x0000ffff}
        },
        {   /* shader 14 */
            "ps_1_4\n"
            "texdepth r5\n",
            {0xffff0104, 0x00000057, 0x800f0005, 0x0000ffff}
        },
    };

    exec_tests("ps_1_4", tests, sizeof(tests) / sizeof(tests[0]));
}

static void vs_2_0_test(void) {
    struct shader_test tests[] = {
        /* Basic instruction tests */
        {   /* shader 0 */
            "vs_2_0\n"
            "mov r0, r1\n",
            {0xfffe0200, 0x02000001, 0x800f0000, 0x80e40001, 0x0000ffff}
        },
        {   /* shader 1 */
            "vs_2_0\n"
            "lrp r0, v0, c0, r1\n",
            {0xfffe0200, 0x04000012, 0x800f0000, 0x90e40000, 0xa0e40000, 0x80e40001,
             0x0000ffff}
        },
        {   /* shader 2 */
            "vs_2_0\n"
            "dp4 oPos, v0, c0\n",
            {0xfffe0200, 0x03000009, 0xc00f0000, 0x90e40000, 0xa0e40000, 0x0000ffff}
        },
        {   /* shader 3 */
            "vs_2_0\n"
            "mov r0, c0[a0.x]\n",
            {0xfffe0200, 0x03000001, 0x800f0000, 0xa0e42000, 0xb0000000, 0x0000ffff}
        },
        {   /* shader 4 */
            "vs_2_0\n"
            "mov r0, c0[a0.y]\n",
            {0xfffe0200, 0x03000001, 0x800f0000, 0xa0e42000, 0xb0550000, 0x0000ffff}
        },
        {   /* shader 5 */
            "vs_2_0\n"
            "mov r0, c0[a0.z]\n",
            {0xfffe0200, 0x03000001, 0x800f0000, 0xa0e42000, 0xb0aa0000, 0x0000ffff}
        },
        {   /* shader 6 */
            "vs_2_0\n"
            "mov r0, c0[a0.w]\n",
            {0xfffe0200, 0x03000001, 0x800f0000, 0xa0e42000, 0xb0ff0000, 0x0000ffff}
        },
        {   /* shader 7 */
            "vs_2_0\n"
            "mov r0, c0[a0.w].x\n",
            {0xfffe0200, 0x03000001, 0x800f0000, 0xa0002000, 0xb0ff0000, 0x0000ffff}
        },
        {   /* shader 8 */
            "vs_2_0\n"
            "mov r0, -c0[a0.w+5].x\n",
            {0xfffe0200, 0x03000001, 0x800f0000, 0xa1002005, 0xb0ff0000, 0x0000ffff}
        },
        {   /* shader 9 */
            "vs_2_0\n"
            "mov r0, c0[a0]\n",
            {0xfffe0200, 0x03000001, 0x800f0000, 0xa0e42000, 0xb0e40000, 0x0000ffff}
        },
        {   /* shader 10 */
            "vs_2_0\n"
            "mov r0, c0[a0.xyww]\n",
            {0xfffe0200, 0x03000001, 0x800f0000, 0xa0e42000, 0xb0f40000, 0x0000ffff}
        },
        {   /* shader 11 */
            "vs_2_0\n"
            "add r0, c0[a0.x], c1[a0.y]\n", /* validation would fail on this line */
            {0xfffe0200, 0x05000002, 0x800f0000, 0xa0e42000, 0xb0000000, 0xa0e42001,
             0xb0550000, 0x0000ffff}
        },
        {   /* shader 12 */
            "vs_2_0\n"
            "rep i0\n"
            "endrep\n",
            {0xfffe0200, 0x01000026, 0xf0e40000, 0x00000027, 0x0000ffff}
        },
        {   /* shader 13 */
            "vs_2_0\n"
            "if b0\n"
            "else\n"
            "endif\n",
            {0xfffe0200, 0x01000028, 0xe0e40800, 0x0000002a, 0x0000002b, 0x0000ffff}
        },
        {   /* shader 14 */
            "vs_2_0\n"
            "loop aL, i0\n"
            "endloop\n",
            {0xfffe0200, 0x0200001b, 0xf0e40800, 0xf0e40000, 0x0000001d, 0x0000ffff}
        },
        {   /* shader 15 */
            "vs_2_0\n"
            "nrm r0, c0\n",
            {0xfffe0200, 0x02000024, 0x800f0000, 0xa0e40000, 0x0000ffff}
        },
        {   /* shader 16 */
            "vs_2_0\n"
            "crs r0, r1, r2\n",
            {0xfffe0200, 0x03000021, 0x800f0000, 0x80e40001, 0x80e40002, 0x0000ffff}
        },
        {   /* shader 17 */
            "vs_2_0\n"
            "sgn r0, r1, r2, r3\n",
            {0xfffe0200, 0x04000022, 0x800f0000, 0x80e40001, 0x80e40002, 0x80e40003,
             0x0000ffff}
        },
        {   /* shader 18 */
            "vs_2_0\n"
            "sincos r0, r1, r2, r3\n",
            {0xfffe0200, 0x04000025, 0x800f0000, 0x80e40001, 0x80e40002, 0x80e40003,
             0x0000ffff}
        },
        {   /* shader 19 */
            "vs_2_0\n"
            "pow r0, r1, r2\n",
            {0xfffe0200, 0x03000020, 0x800f0000, 0x80e40001, 0x80e40002, 0x0000ffff}
        },
        {   /* shader 20 */
            "vs_2_0\n"
            "mova a0.y, c0.z\n",
            {0xfffe0200, 0x0200002e, 0xb0020000, 0xa0aa0000, 0x0000ffff}
        },
        {   /* shader 21 */
            "vs_2_0\n"
            "defb b0, true\n"
            "defb b1, false\n",
            {0xfffe0200, 0x0200002f, 0xe00f0800, 0x00000001, 0x0200002f, 0xe00f0801,
             0x00000000, 0x0000ffff}
        },
        {   /* shader 22 */
            "vs_2_0\n"
            "defi i0, -1, 1, 10, 0\n"
            "defi i1, 0, 40, 30, 10\n",
            {0xfffe0200, 0x05000030, 0xf00f0000, 0xffffffff, 0x00000001, 0x0000000a,
             0x00000000, 0x05000030, 0xf00f0001, 0x00000000, 0x00000028, 0x0000001e,
             0x0000000a, 0x0000ffff}
        },
        {   /* shader 23 */
            "vs_2_0\n"
            "loop aL, i0\n"
            "mov r0, c0[aL]\n"
            "endloop\n",
            {0xfffe0200, 0x0200001b, 0xf0e40800, 0xf0e40000, 0x03000001, 0x800f0000,
             0xa0e42000, 0xf0e40800, 0x0000001d, 0x0000ffff}
        },
        {   /* shader 24 */
            "vs_2_0\n"
            "call l0\n"
            "ret\n"
            "label l0\n"
            "ret\n",
            {0xfffe0200, 0x01000019, 0xa0e41000, 0x0000001c, 0x0100001e, 0xa0e41000,
             0x0000001c, 0x0000ffff}
        },
        {   /* shader 25 */
            "vs_2_0\n"
            "callnz l0, b0\n"
            "ret\n"
            "label l0\n"
            "ret\n",
            {0xfffe0200, 0x0200001a, 0xa0e41000, 0xe0e40800, 0x0000001c, 0x0100001e,
             0xa0e41000, 0x0000001c, 0x0000ffff}
        },
        {   /* shader 26 */
            "vs_2_0\n"
            "callnz l0, !b0\n"
            "ret\n"
            "label l0\n"
            "ret\n",
            {0xfffe0200, 0x0200001a, 0xa0e41000, 0xede40800, 0x0000001c, 0x0100001e,
             0xa0e41000, 0x0000001c, 0x0000ffff}
        },
        {   /* shader 27 */
            "vs_2_0\n"
            "if !b0\n"
            "else\n"
            "endif\n",
            {0xfffe0200, 0x01000028, 0xede40800, 0x0000002a, 0x0000002b, 0x0000ffff}
        },
    };

    exec_tests("vs_2_0", tests, sizeof(tests) / sizeof(tests[0]));
}

static void vs_2_x_test(void) {
    struct shader_test tests[] = {
        {   /* shader 0 */
            "vs_2_x\n"
            "rep i0\n"
            "break\n"
            "endrep\n",
            {0xfffe0201, 0x01000026, 0xf0e40000, 0x0000002c, 0x00000027, 0x0000ffff}
        },
        {   /* shader 1 */
            "vs_2_x\n"
            "if_ge r0, r1\n"
            "endif\n",
            {0xfffe0201, 0x02030029, 0x80e40000, 0x80e40001, 0x0000002b, 0x0000ffff}
        },
        {   /* shader 2 */
            "vs_2_x\n"
            "rep i0\n"
            "break_ne r0, r1\n"
            "endrep",
            {0xfffe0201, 0x01000026, 0xf0e40000, 0x0205002d, 0x80e40000, 0x80e40001,
             0x00000027, 0x0000ffff}
        },

        /* predicates */
        {   /* shader 3 */
            "vs_2_x\n"
            "setp_gt p0, r0, r1\n"
            "(!p0) add r2, r2, r3\n",
            {0xfffe0201, 0x0301005e, 0xb00f1000, 0x80e40000, 0x80e40001, 0x14000002,
             0x800f0002, 0xbde41000, 0x80e40002, 0x80e40003, 0x0000ffff}
        },
        {   /* shader 4 */
            "vs_2_x\n"
            "if p0.x\n"
            "else\n"
            "endif\n",
            {0xfffe0201, 0x01000028, 0xb0001000, 0x0000002a, 0x0000002b, 0x0000ffff}
        },
        {   /* shader 5 */
            "vs_2_x\n"
            "callnz l0, !p0.z\n"
            "ret\n"
            "label l0\n"
            "ret\n",
            {0xfffe0201, 0x0200001a, 0xa0e41000, 0xbdaa1000, 0x0000001c,
             0x0100001e, 0xa0e41000, 0x0000001c, 0x0000ffff}
        },
        {   /* shader 6 */
            "vs_2_x\n"
            "rep i0\n"
            "breakp p0.w\n"
            "endrep\n",
            {0xfffe0201, 0x01000026, 0xf0e40000, 0x01000060, 0xb0ff1000,
             0x00000027, 0x0000ffff}
        },
    };

    exec_tests("vs_2_x", tests, sizeof(tests) / sizeof(tests[0]));
}

static void ps_2_0_test(void) {
    struct shader_test tests[] = {
        {   /* shader 0 */
            "ps_2_0\n"
            "dcl_2d s0\n",
            {0xffff0200, 0x0200001f, 0x90000000, 0xa00f0800, 0x0000ffff}
        },
        {   /* shader 1 */
            "ps_2_0\n"
            "dcl_cube s0\n",
            {0xffff0200, 0x0200001f, 0x98000000, 0xa00f0800, 0x0000ffff}
        },
        {   /* shader 2 */
            "ps_2_0\n"
            "dcl_volume s0\n",
            {0xffff0200, 0x0200001f, 0xa0000000, 0xa00f0800, 0x0000ffff}
        },
        {   /* shader 3 */
            "ps_2_0\n"
            "dcl_volume s0\n"
            "dcl_cube s1\n"
            "dcl_2d s2\n",
            {0xffff0200, 0x0200001f, 0xa0000000, 0xa00f0800, 0x0200001f, 0x98000000,
             0xa00f0801, 0x0200001f, 0x90000000, 0xa00f0802, 0x0000ffff}
        },
        {   /* shader 4 */
            "ps_2_0\n"
            "mov r0, t0\n",
            {0xffff0200, 0x02000001, 0x800f0000, 0xb0e40000, 0x0000ffff}
        },
        {   /* shader 5 */
            "ps_2_0\n"
            "dcl_2d s2\n"
            "texld r0, t1, s2\n",
            {0xffff0200, 0x0200001f, 0x90000000, 0xa00f0802, 0x03000042, 0x800f0000,
             0xb0e40001, 0xa0e40802, 0x0000ffff}
        },
        {   /* shader 6 */
            "ps_2_0\n"
            "texkill t0\n",
            {0xffff0200, 0x01000041, 0xb00f0000, 0x0000ffff}
        },
        {   /* shader 7 */
            "ps_2_0\n"
            "mov oC0, c0\n"
            "mov oC1, c1\n",
            {0xffff0200, 0x02000001, 0x800f0800, 0xa0e40000, 0x02000001, 0x800f0801,
             0xa0e40001, 0x0000ffff}
        },
        {   /* shader 8 */
            "ps_2_0\n"
            "mov oDepth, c0.x\n",
            {0xffff0200, 0x02000001, 0x900f0800, 0xa0000000, 0x0000ffff}
        },
        {   /* shader 9 */
            "ps_2_0\n"
            "dcl_2d s2\n"
            "texldp r0, t1, s2\n",
            {0xffff0200, 0x0200001f, 0x90000000, 0xa00f0802, 0x03010042, 0x800f0000,
             0xb0e40001, 0xa0e40802, 0x0000ffff}
        },
        {   /* shader 10 */
            "ps_2_0\n"
            "dcl_2d s2\n"
            "texldb r0, t1, s2\n",
            {0xffff0200, 0x0200001f, 0x90000000, 0xa00f0802, 0x03020042, 0x800f0000,
             0xb0e40001, 0xa0e40802, 0x0000ffff}
        },
    };

    exec_tests("ps_2_0", tests, sizeof(tests) / sizeof(tests[0]));
}

static void ps_2_x_test(void) {
    struct shader_test tests[] = {
        /* defb and defi are not supposed to work in ps_2_0 (even if defb actually works in ps_2_0 with native) */
        {   /* shader 0 */
            "ps_2_x\n"
            "defb b0, true\n"
            "defb b1, false\n",
            {0xffff0201, 0x0200002f, 0xe00f0800, 0x00000001, 0x0200002f, 0xe00f0801,
             0x00000000, 0x0000ffff}
        },
        {   /* shader 1 */
            "ps_2_x\n"
            "defi i0, -1, 1, 10, 0\n"
            "defi i1, 0, 40, 30, 10\n",
            {0xffff0201, 0x05000030, 0xf00f0000, 0xffffffff, 0x00000001, 0x0000000a,
             0x00000000, 0x05000030, 0xf00f0001, 0x00000000, 0x00000028, 0x0000001e,
             0x0000000a, 0x0000ffff}
        },
        {   /* shader 2 */
            "ps_2_x\n"
            "dsx r0, r0\n",
            {0xffff0201, 0x0200005b, 0x800f0000, 0x80e40000, 0x0000ffff}
        },
        {   /* shader 3 */
            "ps_2_x\n"
            "dsy r0, r0\n",
            {0xffff0201, 0x0200005c, 0x800f0000, 0x80e40000, 0x0000ffff}
        },
        {   /* shader 4 */
            "ps_2_x\n"
            "dcl_2d s2\n"
            "texldd r0, v1, s2, r3, r4\n",
            {0xffff0201, 0x0200001f, 0x90000000, 0xa00f0802, 0x0500005d, 0x800f0000,
             0x90e40001, 0xa0e40802, 0x80e40003, 0x80e40004, 0x0000ffff}
        },
        /* Static flow control tests */
        {   /* shader 5 */
            "ps_2_x\n"
            "call l0\n"
            "ret\n"
            "label l0\n"
            "ret\n",
            {0xffff0201, 0x01000019, 0xa0e41000, 0x0000001c, 0x0100001e, 0xa0e41000,
             0x0000001c, 0x0000ffff}
        },
        {   /* shader 6 */
            "ps_2_x\n"
            "callnz l0, b0\n"
            "ret\n"
            "label l0\n"
            "ret\n",
            {0xffff0201, 0x0200001a, 0xa0e41000, 0xe0e40800, 0x0000001c, 0x0100001e,
             0xa0e41000, 0x0000001c, 0x0000ffff}
        },
        {   /* shader 7 */
            "ps_2_x\n"
            "callnz l0, !b0\n"
            "ret\n"
            "label l0\n"
            "ret\n",
            {0xffff0201, 0x0200001a, 0xa0e41000, 0xede40800, 0x0000001c, 0x0100001e,
             0xa0e41000, 0x0000001c, 0x0000ffff}
        },
        {   /* shader 8 */
            "ps_2_x\n"
            "if !b0\n"
            "else\n"
            "endif\n",
            {0xffff0201, 0x01000028, 0xede40800, 0x0000002a, 0x0000002b, 0x0000ffff}
        },
        /* Dynamic flow control tests */
        {   /* shader 9 */
            "ps_2_x\n"
            "rep i0\n"
            "break\n"
            "endrep\n",
            {0xffff0201, 0x01000026, 0xf0e40000, 0x0000002c, 0x00000027, 0x0000ffff}
        },
        {   /* shader 10 */
            "ps_2_x\n"
            "if_ge r0, r1\n"
            "endif\n",
            {0xffff0201, 0x02030029, 0x80e40000, 0x80e40001, 0x0000002b, 0x0000ffff}
        },
        {   /* shader 11 */
            "ps_2_x\n"
            "rep i0\n"
            "break_ne r0, r1\n"
            "endrep",
            {0xffff0201, 0x01000026, 0xf0e40000, 0x0205002d, 0x80e40000, 0x80e40001,
             0x00000027, 0x0000ffff}
        },
        /* Predicates */
        {   /* shader 12 */
            "ps_2_x\n"
            "setp_gt p0, r0, r1\n"
            "(!p0) add r2, r2, r3\n",
            {0xffff0201, 0x0301005e, 0xb00f1000, 0x80e40000, 0x80e40001, 0x14000002,
             0x800f0002, 0xbde41000, 0x80e40002, 0x80e40003, 0x0000ffff}
        },
        {   /* shader 13 */
            "ps_2_x\n"
            "if p0.x\n"
            "else\n"
            "endif\n",
            {0xffff0201, 0x01000028, 0xb0001000, 0x0000002a, 0x0000002b, 0x0000ffff}
        },
        {   /* shader 14 */
            "ps_2_x\n"
            "callnz l0, !p0.z\n"
            "ret\n"
            "label l0\n"
            "ret\n",
            {0xffff0201, 0x0200001a, 0xa0e41000, 0xbdaa1000, 0x0000001c,
             0x0100001e, 0xa0e41000, 0x0000001c, 0x0000ffff}
        },
        {   /* shader 15 */
            "ps_2_x\n"
            "rep i0\n"
            "breakp p0.w\n"
            "endrep\n",
            {0xffff0201, 0x01000026, 0xf0e40000, 0x01000060, 0xb0ff1000,
             0x00000027, 0x0000ffff}
        },
    };

    exec_tests("ps_2_x", tests, sizeof(tests) / sizeof(tests[0]));
}

static void vs_3_0_test(void) {
    /* FIXME: Some tests are temporarily commented out, because the
       current implementation doesn't support the entire vs_3_0 syntax
       and it is not trivial to remove todo_wine only from
       a subset of the tests here */
    struct shader_test tests[] = {
        {   /* shader 0 */
            "vs_3_0\n"
            "mov r0, c0\n",
            {0xfffe0300, 0x02000001, 0x800f0000, 0xa0e40000, 0x0000ffff}
        },
/*      {*/ /* shader 1 */
/*          "vs_3_0\n"
            "dcl_2d s0\n",
            {0xfffe0300, 0x0200001f, 0x90000000, 0xa00f0800, 0x0000ffff}
        },*/
/*      {*/ /* shader 2 */
/*          "vs_3_0\n"
            "dcl_position o0\n",
            {0xfffe0300, 0x0200001f, 0x80000000, 0xe00f0000, 0x0000ffff}
        },*/
/*      {*/ /* shader 3 */
/*          "vs_3_0\n"
            "dcl_texcoord12 o11\n",
            {0xfffe0300, 0x0200001f, 0x800c0005, 0xe00f000b, 0x0000ffff}
        },*/
/*      {*/ /* shader 4 */
/*          "vs_3_0\n"
            "texldl r0, v0, s0\n",
            {0xfffe0300, 0x0300005f, 0x800f0000, 0x90e40000, 0xa0e40800, 0x0000ffff}
        },*/
/*      {*/ /* shader 5 */
/*          "vs_3_0\n"
            "mov r0, c0[aL]\n",
            {0xfffe0300, 0x03000001, 0x800f0000, 0xa0e42000, 0xf0e40800, 0x0000ffff}
        },*/
/*      {*/ /* shader 6 */
/*          "vs_3_0\n"
            "mov o[ a0.x + 12 ], r0\n",
            {0xfffe0300, 0x03000001, 0xe00f200c, 0xb0000000, 0x80e40000, 0x0000ffff}
        },*/
/*      {*/ /* shader 7 */
/*          "vs_3_0\n"
            "add_sat r0, r0, r1\n",
            {0xfffe0300, 0x03000002, 0x801f0000, 0x80e40000, 0x80e40001, 0x0000ffff}
        },*/
/*      {*/ /* shader 8 */
/*          "vs_3_0\n"
            "mov r2, r1_abs\n",
            {0xfffe0300, 0x02000001, 0x800f0002, 0x8be40001, 0x0000ffff}
        },*/
    };

    exec_tests("vs_3_0", tests, sizeof(tests) / sizeof(tests[0]));
}

static void ps_3_0_test(void) {
    struct shader_test tests[] = {
        {   /* shader 0 */
            "ps_3_0\n"
            "mov r0, c0\n",
            {0xffff0300, 0x02000001, 0x800f0000, 0xa0e40000, 0x0000ffff}
        },
        {   /* shader 1 */
            "ps_3_0\n"
            "dcl_normal5 v0\n",
            {0xffff0300, 0x0200001f, 0x80050003, 0x900f0000, 0x0000ffff}
        },
        {   /* shader 2 */
            "ps_3_0\n"
            "mov r0, vPos\n",
            {0xffff0300, 0x02000001, 0x800f0000, 0x90e41000, 0x0000ffff}
        },
        {   /* shader 3 */
            "ps_3_0\n"
            "mov r0, vFace\n",
            {0xffff0300, 0x02000001, 0x800f0000, 0x90e41001, 0x0000ffff}
        },
        {   /* shader 4 */
            "ps_3_0\n"
            "mov r0, v[ aL + 12 ]\n",
            {0xffff0300, 0x03000001, 0x800f0000, 0x90e4200c, 0xf0e40800, 0x0000ffff}
        },
        {   /* shader 5 */
            "ps_3_0\n"
            "loop aL, i0\n"
            "mov r0, v0[aL]\n"
            "endloop\n",
            {0xffff0300, 0x0200001b, 0xf0e40800, 0xf0e40000, 0x03000001, 0x800f0000,
             0x90e42000, 0xf0e40800, 0x0000001d, 0x0000ffff}
        },
        {   /* shader 6 */
            "ps_3_0\n"
            "texldl r0, v0, s0\n",
            {0xffff0300, 0x0300005f, 0x800f0000, 0x90e40000, 0xa0e40800, 0x0000ffff}
        },
    };

    exec_tests("ps_3_0", tests, sizeof(tests) / sizeof(tests[0]));
}

static void failure_test(void) {
    const char * tests[] = {
        /* shader 0: instruction modifier not allowed */
        "ps_3_0\n"
        "dcl_2d s2\n"
        "texldd_x2 r0, v1, s2, v3, v4\n",
        /* shader 1: coissue not supported in vertex shaders */
        "vs.1.1\r\n"
        "add r0.rgb, r0, r1\n"
        "+add r0.a, r0, r2\n",
        /* shader 2: coissue not supported in pixel shader version >= 2.0 */
        "ps_2_0\n"
        "texld r0, t0, s0\n"
        "add r0.rgb, r0, r1\n"
        "+add r0.a, r0, v1\n",
        /* shader 3: predicates not supported in vertex shader < 2.0 */
        "vs_1_1\n"
        "(p0) add r0, r0, v0\n",
        /* shader 4: register a0 doesn't exist in pixel shaders */
        "ps_3_0\n"
        "mov r0, v[ a0 + 12 ]\n",
        /* shader 5: s0 doesn't exist in vs_1_1 */
        "vs_1_1\n"
        "mov r0, s0\n",
        /* shader 6: aL is a scalar register, no swizzles allowed */
        "ps_3_0\n"
        "mov r0, v[ aL.x + 12 ]\n",
        /* shader 7: tn doesn't exist in ps_3_0 */
        "ps_3_0\n"
        "dcl_2d s2\n"
        "texldd r0, t1, s2, v3, v4\n",
        /* shader 8: two shift modifiers */
        "ps_1_3\n"
        "mov_x2_x2 r0, r1\n",
        /* shader 9: too many source registers for mov instruction */
        "vs_1_1\n"
        "mov r0, r1, r2\n",
        /* shader 10: invalid combination of negate and divide modifiers */
        "ps_1_4\n"
        "texld r5, -r2_dz\n",
        /* shader 11: complement modifier not allowed in >= PS 2 */
        "ps_2_0\n"
        "mov r2, 1 - r0\n",
        /* shader 12: invalid modifier */
        "vs_3_0\n"
        "mov r2, 2 - r0\n",
        /* shader 13: float value in relative addressing */
        "vs_3_0\n"
        "mov r2, c[ aL + 3.4 ]\n",
        /* shader 14: complement modifier not available in VS */
        "vs_3_0\n"
        "mov r2, 1 - r1\n",
        /* shader 15: _x2 modifier not available in VS */
        "vs_1_1\n"
        "mov r2, r1_x2\n",
        /* shader 16: _abs modifier not available in < VS 3.0 */
        "vs_1_1\n"
        "mov r2, r1_abs\n",
        /* shader 17: _x2 modifier not available in >= PS 2.0 */
        "ps_2_0\n"
        "mov r0, r1_x2\n",
        /* shader 18: wrong swizzle */
        "vs_2_0\n"
        "mov r0, r1.abcd\n",
        /* shader 19: wrong swizzle */
        "vs_2_0\n"
        "mov r0, r1.xyzwx\n",
        /* shader 20: wrong swizzle */
        "vs_2_0\n"
        "mov r0, r1.\n",
        /* shader 21: invalid writemask */
        "vs_2_0\n"
        "mov r0.xxyz, r1\n",
        /* shader 22: register r5 doesn't exist in PS < 1.4 */
        "ps_1_3\n"
        "mov r5, r0\n",
    };
    HRESULT hr;
    unsigned int i;
    LPD3DXBUFFER shader,messages;

    for(i = 0; i < (sizeof(tests) / sizeof(tests[0])); i++) {
        shader = NULL;
        messages = NULL;
        hr = D3DXAssembleShader(tests[i], strlen(tests[i]),
                                NULL, NULL, D3DXSHADER_SKIPVALIDATION,
                                &shader, &messages);
        ok(hr == D3DXERR_INVALIDDATA, "Failure test, shader %d: "
           "expected D3DXAssembleShader failure with D3DXERR_INVALIDDATA, "
           "got 0x%x - %d\n", i, hr, hr & 0x0000FFFF);
        if(messages) {
            trace("D3DXAssembleShader messages:\n%s", (char *)ID3DXBuffer_GetBufferPointer(messages));
            ID3DXBuffer_Release(messages);
        }
        if(shader) {
            DWORD *res = ID3DXBuffer_GetBufferPointer(shader);
            dump_shader(res);
            ID3DXBuffer_Release(shader);
        }
    }
}


static HRESULT WINAPI testD3DXInclude_open(ID3DXInclude *iface,
                                           D3DXINCLUDE_TYPE include_type,
                                           LPCSTR filename, LPCVOID parent_data,
                                           LPCVOID *data, UINT *bytes) {
    char *buffer;
    char include[] = "#define REGISTER r0\nvs.1.1\n";

    trace("filename = %s\n",filename);

    buffer = HeapAlloc(GetProcessHeap(), 0, sizeof(include));
    CopyMemory(buffer, include, sizeof(include));
    *data = buffer;
    *bytes = sizeof(include);
    return S_OK;
}

static HRESULT WINAPI testD3DXInclude_close(ID3DXInclude *iface, LPCVOID data) {
    HeapFree(GetProcessHeap(), 0, (LPVOID)data);
    return S_OK;
}

static const struct ID3DXIncludeVtbl D3DXInclude_Vtbl = {
    testD3DXInclude_open,
    testD3DXInclude_close
};

struct D3DXIncludeImpl {
    const ID3DXIncludeVtbl *lpVtbl;
};

static void assembleshader_test(void) {
    const char test1[] = {
        "vs.1.1\n"
        "mov DEF2, v0\n"
    };
    const char testincl[] = {
        "#define REGISTER r0\n"
        "vs.1.1\n"
    };
    const char testshader[] = {
        "#include \"incl.vsh\"\n"
        "mov REGISTER, v0\n"
    };
    HRESULT hr;
    LPD3DXBUFFER shader, messages;
    D3DXMACRO defines[] = {
        {
            "DEF1", "10 + 15"
        },
        {
            "DEF2", "r0"
        },
        {
            NULL, NULL
        }
    };
    struct D3DXIncludeImpl include;
    HRESULT shader_vsh_res, incl_vsh_res;

    todo_wine {

    /* pDefines test */
    shader = NULL;
    messages = NULL;
    hr = D3DXAssembleShader(test1, strlen(test1),
                            defines, NULL, D3DXSHADER_SKIPVALIDATION,
                            &shader, &messages);
    ok(hr == D3D_OK, "pDefines test failed with error 0x%x - %d\n", hr, hr & 0x0000FFFF);
    if(messages) {
        trace("D3DXAssembleShader messages:\n%s", (char *)ID3DXBuffer_GetBufferPointer(messages));
        ID3DXBuffer_Release(messages);
    }
    if(shader) ID3DXBuffer_Release(shader);

    /* pInclude test */
    shader = NULL;
    messages = NULL;
    include.lpVtbl = &D3DXInclude_Vtbl;
    hr = D3DXAssembleShader(testshader, strlen(testshader),
                            NULL, (LPD3DXINCLUDE)&include, D3DXSHADER_SKIPVALIDATION,
                            &shader, &messages);
    ok(hr == D3D_OK, "pInclude test failed with error 0x%x - %d\n", hr, hr & 0x0000FFFF);
    if(messages) {
        trace("D3DXAssembleShader messages:\n%s", (char *)ID3DXBuffer_GetBufferPointer(messages));
        ID3DXBuffer_Release(messages);
    }
    if(shader) ID3DXBuffer_Release(shader);

    shader_vsh_res = create_file("shader.vsh", testshader, sizeof(testshader));
    if(SUCCEEDED(shader_vsh_res)) {
        incl_vsh_res = create_file("incl.vsh", testincl, sizeof(testincl));
        if(SUCCEEDED(incl_vsh_res)) {
            /* D3DXAssembleShaderFromFile + #include test */
            shader = NULL;
            messages = NULL;
            hr = D3DXAssembleShaderFromFileA("shader.vsh",
                                             NULL, NULL, D3DXSHADER_SKIPVALIDATION,
                                             &shader, &messages);
            ok(hr == D3D_OK, "D3DXAssembleShaderFromFile test failed with error 0x%x - %d\n", hr, hr & 0x0000FFFF);
            if(messages) {
                trace("D3DXAssembleShader messages:\n%s", (char *)ID3DXBuffer_GetBufferPointer(messages));
                ID3DXBuffer_Release(messages);
            }
            if(shader) ID3DXBuffer_Release(shader);
        } else skip("Couldn't create \"incl.vsh\"\n");

        /* D3DXAssembleShaderFromFile + pInclude test */
        shader = NULL;
        messages = NULL;
        hr = D3DXAssembleShaderFromFileA("shader.vsh",
                                         NULL, (LPD3DXINCLUDE)&include, D3DXSHADER_SKIPVALIDATION,
                                         &shader, &messages);
        ok(hr == D3D_OK, "D3DXAssembleShaderFromFile + pInclude test failed with error 0x%x - %d\n", hr, hr & 0x0000FFFF);
        if(messages) {
            trace("D3DXAssembleShader messages:\n%s", (char *)ID3DXBuffer_GetBufferPointer(messages));
            ID3DXBuffer_Release(messages);
        }
        if(shader) ID3DXBuffer_Release(shader);
    } else skip("Couldn't create \"shader.vsh\"\n");

    } /* todo_wine */

    /* NULL shader tests */
    shader = NULL;
    messages = NULL;
    hr = D3DXAssembleShader(NULL, 0,
                            NULL, NULL, D3DXSHADER_SKIPVALIDATION,
                            &shader, &messages);
    ok(hr == D3DXERR_INVALIDDATA, "NULL shader test failed with error 0x%x - %d\n", hr, hr & 0x0000FFFF);
    if(messages) {
        trace("D3DXAssembleShader messages:\n%s", (char *)ID3DXBuffer_GetBufferPointer(messages));
        ID3DXBuffer_Release(messages);
    }
    if(shader) ID3DXBuffer_Release(shader);

    todo_wine {

    shader = NULL;
    messages = NULL;
    hr = D3DXAssembleShaderFromFileA("nonexistent.vsh",
                                     NULL, NULL, D3DXSHADER_SKIPVALIDATION,
                                     &shader, &messages);
    ok(hr == D3DXERR_INVALIDDATA || hr == E_FAIL, /* I get this on WinXP */
        "D3DXAssembleShaderFromFile nonexistent file test failed with error 0x%x - %d\n",
        hr, hr & 0x0000FFFF);
    if(messages) {
        trace("D3DXAssembleShaderFromFile messages:\n%s", (char *)ID3DXBuffer_GetBufferPointer(messages));
        ID3DXBuffer_Release(messages);
    }
    if(shader) ID3DXBuffer_Release(shader);

    /* D3DXAssembleShaderFromResource test */
    shader = NULL;
    messages = NULL;
    hr = D3DXAssembleShaderFromResourceA(NULL, MAKEINTRESOURCEA(IDB_ASMSHADER),
                                         NULL, NULL, D3DXSHADER_SKIPVALIDATION,
                                         &shader, &messages);
    ok(hr == D3D_OK, "D3DXAssembleShaderFromResource test failed with error 0x%x - %d\n", hr, hr & 0x0000FFFF);
    if(messages) {
        trace("D3DXAssembleShaderFromResource messages:\n%s", (char *)ID3DXBuffer_GetBufferPointer(messages));
        ID3DXBuffer_Release(messages);
    }
    if(shader) ID3DXBuffer_Release(shader);

    } /* end of todo_wine */

    /* D3DXAssembleShaderFromResource with missing shader resource test */
    shader = NULL;
    messages = NULL;
    hr = D3DXAssembleShaderFromResourceA(NULL, "notexisting",
                                         NULL, NULL, D3DXSHADER_SKIPVALIDATION,
                                         &shader, &messages);
    ok(hr == D3DXERR_INVALIDDATA, "D3DXAssembleShaderFromResource NULL shader test failed with error 0x%x - %d\n", hr, hr & 0x0000FFFF);
    if(messages) {
        trace("D3DXAssembleShaderFromResource messages:\n%s", (char *)ID3DXBuffer_GetBufferPointer(messages));
        ID3DXBuffer_Release(messages);
    }
    if(shader) ID3DXBuffer_Release(shader);

    /* cleanup */
    if(SUCCEEDED(shader_vsh_res)) {
        DeleteFileA("shader.vsh");
        if(SUCCEEDED(incl_vsh_res)) DeleteFileA("incl.vsh");
    }
}

START_TEST(asm)
{
    todo_wine preproc_test();
    todo_wine ps_1_1_test();
    todo_wine vs_1_1_test();
    todo_wine ps_1_3_test();
    todo_wine ps_1_4_test();
    todo_wine vs_2_0_test();
    todo_wine vs_2_x_test();
    todo_wine ps_2_0_test();
    todo_wine ps_2_x_test();
    vs_3_0_test();
    todo_wine ps_3_0_test();

    failure_test();

    assembleshader_test();
}
