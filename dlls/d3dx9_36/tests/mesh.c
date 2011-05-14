/*
 * Copyright 2008 David Adam
 * Copyright 2008 Luis Busquets
 * Copyright 2009 Henri Verbeet for CodeWeavers
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

#include <stdio.h>
#include <float.h>
#include "wine/test.h"
#include "d3dx9.h"

#define admitted_error 0.0001f

#define ARRAY_SIZE(array) (sizeof(array)/sizeof(*array))

#define compare_vertex_sizes(type, exp) \
    got=D3DXGetFVFVertexSize(type); \
    ok(got==exp, "Expected: %d, Got: %d\n", exp, got);

#define compare_float(got, exp) \
    do { \
        float _got = (got); \
        float _exp = (exp); \
        ok(_got == _exp, "Expected: %g, Got: %g\n", _exp, _got); \
    } while (0)

static BOOL compare(FLOAT u, FLOAT v)
{
    return (fabs(u-v) < admitted_error);
}

static BOOL compare_vec3(D3DXVECTOR3 u, D3DXVECTOR3 v)
{
    return ( compare(u.x, v.x) && compare(u.y, v.y) && compare(u.z, v.z) );
}

struct vertex
{
    D3DXVECTOR3 position;
    D3DXVECTOR3 normal;
};

typedef WORD face[3];

static BOOL compare_face(face a, face b)
{
    return (a[0]==b[0] && a[1] == b[1] && a[2] == b[2]);
}

struct mesh
{
    DWORD number_of_vertices;
    struct vertex *vertices;

    DWORD number_of_faces;
    face *faces;

    DWORD fvf;
    UINT vertex_size;
};

static void free_mesh(struct mesh *mesh)
{
    HeapFree(GetProcessHeap(), 0, mesh->faces);
    HeapFree(GetProcessHeap(), 0, mesh->vertices);
}

static BOOL new_mesh(struct mesh *mesh, DWORD number_of_vertices, DWORD number_of_faces)
{
    mesh->vertices = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, number_of_vertices * sizeof(*mesh->vertices));
    if (!mesh->vertices)
    {
        return FALSE;
    }
    mesh->number_of_vertices = number_of_vertices;

    mesh->faces = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, number_of_faces * sizeof(*mesh->faces));
    if (!mesh->faces)
    {
        HeapFree(GetProcessHeap(), 0, mesh->vertices);
        return FALSE;
    }
    mesh->number_of_faces = number_of_faces;

    return TRUE;
}

static void compare_mesh(const char *name, ID3DXMesh *d3dxmesh, struct mesh *mesh)
{
    HRESULT hr;
    DWORD number_of_vertices, number_of_faces;
    IDirect3DVertexBuffer9 *vertex_buffer;
    IDirect3DIndexBuffer9 *index_buffer;
    D3DVERTEXBUFFER_DESC vertex_buffer_description;
    D3DINDEXBUFFER_DESC index_buffer_description;
    struct vertex *vertices;
    face *faces;
    int expected, i;

    number_of_vertices = d3dxmesh->lpVtbl->GetNumVertices(d3dxmesh);
    ok(number_of_vertices == mesh->number_of_vertices, "Test %s, result %u, expected %d\n",
       name, number_of_vertices, mesh->number_of_vertices);

    number_of_faces = d3dxmesh->lpVtbl->GetNumFaces(d3dxmesh);
    ok(number_of_faces == mesh->number_of_faces, "Test %s, result %u, expected %d\n",
       name, number_of_faces, mesh->number_of_faces);

    /* vertex buffer */
    hr = d3dxmesh->lpVtbl->GetVertexBuffer(d3dxmesh, &vertex_buffer);
    ok(hr == D3D_OK, "Test %s, result %x, expected 0 (D3D_OK)\n", name, hr);

    if (hr != D3D_OK)
    {
        skip("Couldn't get vertex buffer\n");
    }
    else
    {
        hr = IDirect3DVertexBuffer9_GetDesc(vertex_buffer, &vertex_buffer_description);
        ok(hr == D3D_OK, "Test %s, result %x, expected 0 (D3D_OK)\n", name, hr);

        if (hr != D3D_OK)
        {
            skip("Couldn't get vertex buffer description\n");
        }
        else
        {
            ok(vertex_buffer_description.Format == D3DFMT_VERTEXDATA, "Test %s, result %x, expected %x (D3DFMT_VERTEXDATA)\n",
               name, vertex_buffer_description.Format, D3DFMT_VERTEXDATA);
            ok(vertex_buffer_description.Type == D3DRTYPE_VERTEXBUFFER, "Test %s, result %x, expected %x (D3DRTYPE_VERTEXBUFFER)\n",
               name, vertex_buffer_description.Type, D3DRTYPE_VERTEXBUFFER);
            ok(vertex_buffer_description.Usage == 0, "Test %s, result %x, expected %x\n", name, vertex_buffer_description.Usage, 0);
            ok(vertex_buffer_description.Pool == D3DPOOL_MANAGED, "Test %s, result %x, expected %x (D3DPOOL_DEFAULT)\n",
               name, vertex_buffer_description.Pool, D3DPOOL_DEFAULT);
            ok(vertex_buffer_description.FVF == mesh->fvf, "Test %s, result %x, expected %x\n",
               name, vertex_buffer_description.FVF, mesh->fvf);
            if (mesh->fvf == 0)
            {
                expected = number_of_vertices * mesh->vertex_size;
            }
            else
            {
                expected = number_of_vertices * D3DXGetFVFVertexSize(mesh->fvf);
            }
            ok(vertex_buffer_description.Size == expected, "Test %s, result %x, expected %x\n",
               name, vertex_buffer_description.Size, expected);
        }

        /* specify offset and size to avoid potential overruns */
        hr = IDirect3DVertexBuffer9_Lock(vertex_buffer, 0, number_of_vertices * sizeof(D3DXVECTOR3) * 2,
                                         (LPVOID *)&vertices, D3DLOCK_DISCARD);
        ok(hr == D3D_OK, "Test %s, result %x, expected 0 (D3D_OK)\n", name, hr);

        if (hr != D3D_OK)
        {
            skip("Couldn't lock vertex buffer\n");
        }
        else
        {
            for (i = 0; i < number_of_vertices; i++)
            {
                ok(compare_vec3(vertices[i].position, mesh->vertices[i].position),
                   "Test %s, vertex position %d, result (%g, %g, %g), expected (%g, %g, %g)\n", name, i,
                   vertices[i].position.x, vertices[i].position.y, vertices[i].position.z,
                   mesh->vertices[i].position.x, mesh->vertices[i].position.y, mesh->vertices[i].position.z);
                ok(compare_vec3(vertices[i].normal, mesh->vertices[i].normal),
                   "Test %s, vertex normal %d, result (%g, %g, %g), expected (%g, %g, %g)\n", name, i,
                   vertices[i].normal.x, vertices[i].normal.y, vertices[i].normal.z,
                   mesh->vertices[i].normal.x, mesh->vertices[i].normal.y, mesh->vertices[i].normal.z);
            }

            IDirect3DVertexBuffer9_Unlock(vertex_buffer);
        }

        IDirect3DVertexBuffer9_Release(vertex_buffer);
    }

    /* index buffer */
    hr = d3dxmesh->lpVtbl->GetIndexBuffer(d3dxmesh, &index_buffer);
    ok(hr == D3D_OK, "Test %s, result %x, expected 0 (D3D_OK)\n", name, hr);

    if (!index_buffer)
    {
        skip("Couldn't get index buffer\n");
    }
    else
    {
        hr = IDirect3DIndexBuffer9_GetDesc(index_buffer, &index_buffer_description);
        ok(hr == D3D_OK, "Test %s, result %x, expected 0 (D3D_OK)\n", name, hr);

        if (hr != D3D_OK)
        {
            skip("Couldn't get index buffer description\n");
        }
        else
        {
            ok(index_buffer_description.Format == D3DFMT_INDEX16, "Test %s, result %x, expected %x (D3DFMT_INDEX16)\n",
               name, index_buffer_description.Format, D3DFMT_INDEX16);
            ok(index_buffer_description.Type == D3DRTYPE_INDEXBUFFER, "Test %s, result %x, expected %x (D3DRTYPE_INDEXBUFFER)\n",
               name, index_buffer_description.Type, D3DRTYPE_INDEXBUFFER);
            todo_wine ok(index_buffer_description.Usage == 0, "Test %s, result %x, expected %x\n", name, index_buffer_description.Usage, 0);
            ok(index_buffer_description.Pool == D3DPOOL_MANAGED, "Test %s, result %x, expected %x (D3DPOOL_DEFAULT)\n",
               name, index_buffer_description.Pool, D3DPOOL_DEFAULT);
            expected = number_of_faces * sizeof(WORD) * 3;
            ok(index_buffer_description.Size == expected, "Test %s, result %x, expected %x\n",
               name, index_buffer_description.Size, expected);
        }

        /* specify offset and size to avoid potential overruns */
        hr = IDirect3DIndexBuffer9_Lock(index_buffer, 0, number_of_faces * sizeof(WORD) * 3,
                                        (LPVOID *)&faces, D3DLOCK_DISCARD);
        ok(hr == D3D_OK, "Test %s, result %x, expected 0 (D3D_OK)\n", name, hr);

        if (hr != D3D_OK)
        {
            skip("Couldn't lock index buffer\n");
        }
        else
        {
            for (i = 0; i < number_of_faces; i++)
            {
                ok(compare_face(faces[i], mesh->faces[i]),
                   "Test %s, face %d, result (%u, %u, %u), expected (%u, %u, %u)\n", name, i,
                   faces[i][0], faces[i][1], faces[i][2],
                   mesh->faces[i][0], mesh->faces[i][1], mesh->faces[i][2]);
            }

            IDirect3DIndexBuffer9_Unlock(index_buffer);
        }

        IDirect3DIndexBuffer9_Release(index_buffer);
    }
}

static void D3DXBoundProbeTest(void)
{
    BOOL result;
    D3DXVECTOR3 bottom_point, center, top_point, raydirection, rayposition;
    FLOAT radius;

/*____________Test the Box case___________________________*/
    bottom_point.x = -3.0f; bottom_point.y = -2.0f; bottom_point.z = -1.0f;
    top_point.x = 7.0f; top_point.y = 8.0f; top_point.z = 9.0f;

    raydirection.x = -4.0f; raydirection.y = -5.0f; raydirection.z = -6.0f;
    rayposition.x = 5.0f; rayposition.y = 5.0f; rayposition.z = 11.0f;
    result = D3DXBoxBoundProbe(&bottom_point, &top_point, &rayposition, &raydirection);
    ok(result == TRUE, "expected TRUE, received FALSE\n");

    raydirection.x = 4.0f; raydirection.y = 5.0f; raydirection.z = 6.0f;
    rayposition.x = 5.0f; rayposition.y = 5.0f; rayposition.z = 11.0f;
    result = D3DXBoxBoundProbe(&bottom_point, &top_point, &rayposition, &raydirection);
    ok(result == FALSE, "expected FALSE, received TRUE\n");

    rayposition.x = -4.0f; rayposition.y = 1.0f; rayposition.z = -2.0f;
    result = D3DXBoxBoundProbe(&bottom_point, &top_point, &rayposition, &raydirection);
    ok(result == TRUE, "expected TRUE, received FALSE\n");

    bottom_point.x = 1.0f; bottom_point.y = 0.0f; bottom_point.z = 0.0f;
    top_point.x = 1.0f; top_point.y = 0.0f; top_point.z = 0.0f;
    rayposition.x = 0.0f; rayposition.y = 1.0f; rayposition.z = 0.0f;
    raydirection.x = 0.0f; raydirection.y = 3.0f; raydirection.z = 0.0f;
    result = D3DXBoxBoundProbe(&bottom_point, &top_point, &rayposition, &raydirection);
    ok(result == FALSE, "expected FALSE, received TRUE\n");

    bottom_point.x = 1.0f; bottom_point.y = 2.0f; bottom_point.z = 3.0f;
    top_point.x = 10.0f; top_point.y = 15.0f; top_point.z = 20.0f;

    raydirection.x = 7.0f; raydirection.y = 8.0f; raydirection.z = 9.0f;
    rayposition.x = 3.0f; rayposition.y = 7.0f; rayposition.z = -6.0f;
    result = D3DXBoxBoundProbe(&bottom_point, &top_point, &rayposition, &raydirection);
    ok(result == TRUE, "expected TRUE, received FALSE\n");

    bottom_point.x = 0.0f; bottom_point.y = 0.0f; bottom_point.z = 0.0f;
    top_point.x = 1.0f; top_point.y = 1.0f; top_point.z = 1.0f;

    raydirection.x = 0.0f; raydirection.y = 1.0f; raydirection.z = .0f;
    rayposition.x = -3.0f; rayposition.y = 0.0f; rayposition.z = 0.0f;
    result = D3DXBoxBoundProbe(&bottom_point, &top_point, &rayposition, &raydirection);
    ok(result == FALSE, "expected FALSE, received TRUE\n");

    raydirection.x = 1.0f; raydirection.y = 0.0f; raydirection.z = .0f;
    rayposition.x = -3.0f; rayposition.y = 0.0f; rayposition.z = 0.0f;
    result = D3DXBoxBoundProbe(&bottom_point, &top_point, &rayposition, &raydirection);
    ok(result == TRUE, "expected TRUE, received FALSE\n");

/*____________Test the Sphere case________________________*/
    radius = sqrt(77.0f);
    center.x = 1.0f; center.y = 2.0f; center.z = 3.0f;
    raydirection.x = 2.0f; raydirection.y = -4.0f; raydirection.z = 2.0f;

    rayposition.x = 5.0f; rayposition.y = 5.0f; rayposition.z = 9.0f;
    result = D3DXSphereBoundProbe(&center, radius, &rayposition, &raydirection);
    ok(result == TRUE, "expected TRUE, received FALSE\n");

    rayposition.x = 45.0f; rayposition.y = -75.0f; rayposition.z = 49.0f;
    result = D3DXSphereBoundProbe(&center, radius, &rayposition, &raydirection);
    ok(result == FALSE, "expected FALSE, received TRUE\n");

    rayposition.x = 5.0f; rayposition.y = 11.0f; rayposition.z = 9.0f;
    result = D3DXSphereBoundProbe(&center, radius, &rayposition, &raydirection);
    ok(result == FALSE, "expected FALSE, received TRUE\n");
}

static void D3DXComputeBoundingBoxTest(void)
{
    D3DXVECTOR3 exp_max, exp_min, got_max, got_min, vertex[5];
    HRESULT hr;

    vertex[0].x = 1.0f; vertex[0].y = 1.0f; vertex[0].z = 1.0f;
    vertex[1].x = 1.0f; vertex[1].y = 1.0f; vertex[1].z = 1.0f;
    vertex[2].x = 1.0f; vertex[2].y = 1.0f; vertex[2].z = 1.0f;
    vertex[3].x = 1.0f; vertex[3].y = 1.0f; vertex[3].z = 1.0f;
    vertex[4].x = 9.0f; vertex[4].y = 9.0f; vertex[4].z = 9.0f;

    exp_min.x = 1.0f; exp_min.y = 1.0f; exp_min.z = 1.0f;
    exp_max.x = 9.0f; exp_max.y = 9.0f; exp_max.z = 9.0f;

    hr = D3DXComputeBoundingBox(&vertex[3],2,D3DXGetFVFVertexSize(D3DFVF_XYZ),&got_min,&got_max);

    ok( hr == D3D_OK, "Expected D3D_OK, got %#x\n", hr);
    ok( compare_vec3(exp_min,got_min), "Expected min: (%f, %f, %f), got: (%f, %f, %f)\n", exp_min.x,exp_min.y,exp_min.z,got_min.x,got_min.y,got_min.z);
    ok( compare_vec3(exp_max,got_max), "Expected max: (%f, %f, %f), got: (%f, %f, %f)\n", exp_max.x,exp_max.y,exp_max.z,got_max.x,got_max.y,got_max.z);

/*________________________*/

    vertex[0].x = 2.0f; vertex[0].y = 5.9f; vertex[0].z = -1.2f;
    vertex[1].x = -1.87f; vertex[1].y = 7.9f; vertex[1].z = 7.4f;
    vertex[2].x = 7.43f; vertex[2].y = -0.9f; vertex[2].z = 11.9f;
    vertex[3].x = -6.92f; vertex[3].y = 6.3f; vertex[3].z = -3.8f;
    vertex[4].x = 11.4f; vertex[4].y = -8.1f; vertex[4].z = 4.5f;

    exp_min.x = -6.92f; exp_min.y = -8.1f; exp_min.z = -3.80f;
    exp_max.x = 11.4f; exp_max.y = 7.90f; exp_max.z = 11.9f;

    hr = D3DXComputeBoundingBox(&vertex[0],5,D3DXGetFVFVertexSize(D3DFVF_XYZ),&got_min,&got_max);

    ok( hr == D3D_OK, "Expected D3D_OK, got %#x\n", hr);
    ok( compare_vec3(exp_min,got_min), "Expected min: (%f, %f, %f), got: (%f, %f, %f)\n", exp_min.x,exp_min.y,exp_min.z,got_min.x,got_min.y,got_min.z);
    ok( compare_vec3(exp_max,got_max), "Expected max: (%f, %f, %f), got: (%f, %f, %f)\n", exp_max.x,exp_max.y,exp_max.z,got_max.x,got_max.y,got_max.z);

/*________________________*/

    vertex[0].x = 2.0f; vertex[0].y = 5.9f; vertex[0].z = -1.2f;
    vertex[1].x = -1.87f; vertex[1].y = 7.9f; vertex[1].z = 7.4f;
    vertex[2].x = 7.43f; vertex[2].y = -0.9f; vertex[2].z = 11.9f;
    vertex[3].x = -6.92f; vertex[3].y = 6.3f; vertex[3].z = -3.8f;
    vertex[4].x = 11.4f; vertex[4].y = -8.1f; vertex[4].z = 4.5f;

    exp_min.x = -6.92f; exp_min.y = -0.9f; exp_min.z = -3.8f;
    exp_max.x = 7.43f; exp_max.y = 7.90f; exp_max.z = 11.9f;

    hr = D3DXComputeBoundingBox(&vertex[0],4,D3DXGetFVFVertexSize(D3DFVF_XYZ),&got_min,&got_max);

    ok( hr == D3D_OK, "Expected D3D_OK, got %#x\n", hr);
    ok( compare_vec3(exp_min,got_min), "Expected min: (%f, %f, %f), got: (%f, %f, %f)\n", exp_min.x,exp_min.y,exp_min.z,got_min.x,got_min.y,got_min.z);
    ok( compare_vec3(exp_max,got_max), "Expected max: (%f, %f, %f), got: (%f, %f, %f)\n", exp_max.x,exp_max.y,exp_max.z,got_max.x,got_max.y,got_max.z);

/*________________________*/
    hr = D3DXComputeBoundingBox(NULL,5,D3DXGetFVFVertexSize(D3DFVF_XYZ),&got_min,&got_max);
    ok( hr == D3DERR_INVALIDCALL, "Expected D3DERR_INVALIDCALL, got %#x\n", hr);

/*________________________*/
    hr = D3DXComputeBoundingBox(&vertex[3],5,D3DXGetFVFVertexSize(D3DFVF_XYZ),NULL,&got_max);
    ok( hr == D3DERR_INVALIDCALL, "Expected D3DERR_INVALIDCALL, got %#x\n", hr);

/*________________________*/
    hr = D3DXComputeBoundingBox(&vertex[3],5,D3DXGetFVFVertexSize(D3DFVF_XYZ),&got_min,NULL);
    ok( hr == D3DERR_INVALIDCALL, "Expected D3DERR_INVALIDCALL, got %#x\n", hr);
}

static void D3DXComputeBoundingSphereTest(void)
{
    D3DXVECTOR3 exp_cen, got_cen, vertex[5];
    FLOAT exp_rad, got_rad;
    HRESULT hr;

    vertex[0].x = 1.0f; vertex[0].y = 1.0f; vertex[0].z = 1.0f;
    vertex[1].x = 1.0f; vertex[1].y = 1.0f; vertex[1].z = 1.0f;
    vertex[2].x = 1.0f; vertex[2].y = 1.0f; vertex[2].z = 1.0f;
    vertex[3].x = 1.0f; vertex[3].y = 1.0f; vertex[3].z = 1.0f;
    vertex[4].x = 9.0f; vertex[4].y = 9.0f; vertex[4].z = 9.0f;

    exp_rad = 6.928203f;
    exp_cen.x = 5.0; exp_cen.y = 5.0; exp_cen.z = 5.0;

    hr = D3DXComputeBoundingSphere(&vertex[3],2,D3DXGetFVFVertexSize(D3DFVF_XYZ),&got_cen,&got_rad);

    ok( hr == D3D_OK, "Expected D3D_OK, got %#x\n", hr);
    ok( compare(exp_rad, got_rad), "Expected radius: %f, got radius: %f\n", exp_rad, got_rad);
    ok( compare_vec3(exp_cen,got_cen), "Expected center: (%f, %f, %f), got center: (%f, %f, %f)\n", exp_cen.x,exp_cen.y,exp_cen.z,got_cen.x,got_cen.y,got_cen.z);

/*________________________*/

    vertex[0].x = 2.0f; vertex[0].y = 5.9f; vertex[0].z = -1.2f;
    vertex[1].x = -1.87f; vertex[1].y = 7.9f; vertex[1].z = 7.4f;
    vertex[2].x = 7.43f; vertex[2].y = -0.9f; vertex[2].z = 11.9f;
    vertex[3].x = -6.92f; vertex[3].y = 6.3f; vertex[3].z = -3.8f;
    vertex[4].x = 11.4f; vertex[4].y = -8.1f; vertex[4].z = 4.5f;

    exp_rad = 13.707883f;
    exp_cen.x = 2.408f; exp_cen.y = 2.22f; exp_cen.z = 3.76f;

    hr = D3DXComputeBoundingSphere(&vertex[0],5,D3DXGetFVFVertexSize(D3DFVF_XYZ),&got_cen,&got_rad);

    ok( hr == D3D_OK, "Expected D3D_OK, got %#x\n", hr);
    ok( compare(exp_rad, got_rad), "Expected radius: %f, got radius: %f\n", exp_rad, got_rad);
    ok( compare_vec3(exp_cen,got_cen), "Expected center: (%f, %f, %f), got center: (%f, %f, %f)\n", exp_cen.x,exp_cen.y,exp_cen.z,got_cen.x,got_cen.y,got_cen.z);

/*________________________*/
    hr = D3DXComputeBoundingSphere(NULL,5,D3DXGetFVFVertexSize(D3DFVF_XYZ),&got_cen,&got_rad);
    ok( hr == D3DERR_INVALIDCALL, "Expected D3DERR_INVALIDCALL, got %#x\n", hr);

/*________________________*/
    hr = D3DXComputeBoundingSphere(&vertex[3],5,D3DXGetFVFVertexSize(D3DFVF_XYZ),NULL,&got_rad);
    ok( hr == D3DERR_INVALIDCALL, "Expected D3DERR_INVALIDCALL, got %#x\n", hr);

/*________________________*/
    hr = D3DXComputeBoundingSphere(&vertex[3],5,D3DXGetFVFVertexSize(D3DFVF_XYZ),&got_cen,NULL);
    ok( hr == D3DERR_INVALIDCALL, "Expected D3DERR_INVALIDCALL, got %#x\n", hr);
}

static void print_elements(const D3DVERTEXELEMENT9 *elements)
{
    D3DVERTEXELEMENT9 last = D3DDECL_END();
    const D3DVERTEXELEMENT9 *ptr = elements;
    int count = 0;

    while (memcmp(ptr, &last, sizeof(D3DVERTEXELEMENT9)))
    {
        trace(
            "[Element %d] Stream = %d, Offset = %d, Type = %d, Method = %d, Usage = %d, UsageIndex = %d\n",
             count, ptr->Stream, ptr->Offset, ptr->Type, ptr->Method, ptr->Usage, ptr->UsageIndex);
        ptr++;
        count++;
    }
}

static void compare_elements(const D3DVERTEXELEMENT9 *elements, const D3DVERTEXELEMENT9 *expected_elements,
        unsigned int line, unsigned int test_id)
{
    D3DVERTEXELEMENT9 last = D3DDECL_END();
    unsigned int i;

    for (i = 0; i < MAX_FVF_DECL_SIZE; i++)
    {
        int end1 = memcmp(&elements[i], &last, sizeof(last));
        int end2 = memcmp(&expected_elements[i], &last, sizeof(last));
        int status;

        if (!end1 && !end2) break;

        status = !end1 ^ !end2;
        ok(!status, "Line %u, test %u: Mismatch in size, test declaration is %s than expected.\n",
                line, test_id, end1 ? "shorter" : "longer");
        if (status)
        {
            print_elements(elements);
            break;
        }

        status = memcmp(&elements[i], &expected_elements[i], sizeof(D3DVERTEXELEMENT9));
        ok(!status, "Line %u, test %u: Mismatch in element %u.\n", line, test_id, i);
        if (status)
        {
            print_elements(elements);
            break;
        }
    }
}

static void test_fvf_to_decl(DWORD test_fvf, const D3DVERTEXELEMENT9 expected_elements[],
        HRESULT expected_hr, unsigned int line, unsigned int test_id)
{
    HRESULT hr;
    D3DVERTEXELEMENT9 decl[MAX_FVF_DECL_SIZE];

    hr = D3DXDeclaratorFromFVF(test_fvf, decl);
    ok(hr == expected_hr,
            "Line %u, test %u: D3DXDeclaratorFromFVF returned %#x, expected %#x.\n",
            line, test_id, hr, expected_hr);
    if (SUCCEEDED(hr)) compare_elements(decl, expected_elements, line, test_id);
}

static void test_decl_to_fvf(const D3DVERTEXELEMENT9 *decl, DWORD expected_fvf,
        HRESULT expected_hr, unsigned int line, unsigned int test_id)
{
    HRESULT hr;
    DWORD result_fvf = 0xdeadbeef;

    hr = D3DXFVFFromDeclarator(decl, &result_fvf);
    ok(hr == expected_hr,
       "Line %u, test %u: D3DXFVFFromDeclarator returned %#x, expected %#x.\n",
       line, test_id, hr, expected_hr);
    if (SUCCEEDED(hr))
    {
        ok(expected_fvf == result_fvf, "Line %u, test %u: Got FVF %#x, expected %#x.\n",
                line, test_id, result_fvf, expected_fvf);
    }
}

static void test_fvf_decl_conversion(void)
{
    static const struct
    {
        D3DVERTEXELEMENT9 decl[MAXD3DDECLLENGTH + 1];
        DWORD fvf;
    }
    test_data[] =
    {
        {{
            D3DDECL_END(),
        }, 0},
        {{
            {0, 0, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_POSITION, 0},
            D3DDECL_END(),
        }, D3DFVF_XYZ},
        {{
            {0, 0, D3DDECLTYPE_FLOAT4, 0, D3DDECLUSAGE_POSITIONT, 0},
            D3DDECL_END(),
        }, D3DFVF_XYZRHW},
        {{
            {0, 0, D3DDECLTYPE_FLOAT4, 0, D3DDECLUSAGE_POSITIONT, 0},
            D3DDECL_END(),
        }, D3DFVF_XYZRHW},
        {{
            {0, 0, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_POSITION, 0},
            {0, 12, D3DDECLTYPE_FLOAT1, 0, D3DDECLUSAGE_BLENDWEIGHT, 0},
            D3DDECL_END(),
        }, D3DFVF_XYZB1},
        {{
            {0, 0, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_POSITION, 0},
            {0, 12, D3DDECLTYPE_UBYTE4, 0, D3DDECLUSAGE_BLENDINDICES, 0},
            D3DDECL_END(),
        }, D3DFVF_XYZB1 | D3DFVF_LASTBETA_UBYTE4},
        {{
            {0, 0, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_POSITION, 0},
            {0, 12, D3DDECLTYPE_D3DCOLOR, 0, D3DDECLUSAGE_BLENDINDICES, 0},
            D3DDECL_END(),
        }, D3DFVF_XYZB1 | D3DFVF_LASTBETA_D3DCOLOR},
        {{
            {0, 0, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_POSITION, 0},
            {0, 12, D3DDECLTYPE_FLOAT2, 0, D3DDECLUSAGE_BLENDWEIGHT, 0},
            D3DDECL_END(),
        }, D3DFVF_XYZB2},
        {{
            {0, 0, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_POSITION, 0},
            {0, 12, D3DDECLTYPE_FLOAT1, 0, D3DDECLUSAGE_BLENDWEIGHT, 0},
            {0, 16, D3DDECLTYPE_UBYTE4, 0, D3DDECLUSAGE_BLENDINDICES, 0},
            D3DDECL_END(),
        }, D3DFVF_XYZB2 | D3DFVF_LASTBETA_UBYTE4},
        {{
            {0, 0, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_POSITION, 0},
            {0, 12, D3DDECLTYPE_FLOAT1, 0, D3DDECLUSAGE_BLENDWEIGHT, 0},
            {0, 16, D3DDECLTYPE_D3DCOLOR, 0, D3DDECLUSAGE_BLENDINDICES, 0},
            D3DDECL_END(),
        }, D3DFVF_XYZB2 | D3DFVF_LASTBETA_D3DCOLOR},
        {{
            {0, 0, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_POSITION, 0},
            {0, 12, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_BLENDWEIGHT, 0},
            D3DDECL_END(),
        }, D3DFVF_XYZB3},
        {{
            {0, 0, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_POSITION, 0},
            {0, 12, D3DDECLTYPE_FLOAT2, 0, D3DDECLUSAGE_BLENDWEIGHT, 0},
            {0, 20, D3DDECLTYPE_UBYTE4, 0, D3DDECLUSAGE_BLENDINDICES, 0},
            D3DDECL_END(),
        }, D3DFVF_XYZB3 | D3DFVF_LASTBETA_UBYTE4},
        {{
            {0, 0, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_POSITION, 0},
            {0, 12, D3DDECLTYPE_FLOAT2, 0, D3DDECLUSAGE_BLENDWEIGHT, 0},
            {0, 20, D3DDECLTYPE_D3DCOLOR, 0, D3DDECLUSAGE_BLENDINDICES, 0},
            D3DDECL_END(),
        }, D3DFVF_XYZB3 | D3DFVF_LASTBETA_D3DCOLOR},
        {{
            {0, 0, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_POSITION, 0},
            {0, 12, D3DDECLTYPE_FLOAT4, 0, D3DDECLUSAGE_BLENDWEIGHT, 0},
            D3DDECL_END(),
        }, D3DFVF_XYZB4},
        {{
            {0, 0, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_POSITION, 0},
            {0, 12, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_BLENDWEIGHT, 0},
            {0, 24, D3DDECLTYPE_UBYTE4, 0, D3DDECLUSAGE_BLENDINDICES, 0},
            D3DDECL_END(),
        }, D3DFVF_XYZB4 | D3DFVF_LASTBETA_UBYTE4},
        {{
            {0, 0, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_POSITION, 0},
            {0, 12, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_BLENDWEIGHT, 0},
            {0, 24, D3DDECLTYPE_D3DCOLOR, 0, D3DDECLUSAGE_BLENDINDICES, 0},
            D3DDECL_END(),
        }, D3DFVF_XYZB4 | D3DFVF_LASTBETA_D3DCOLOR},
        {{
            {0, 0, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_POSITION, 0},
            {0, 12, D3DDECLTYPE_FLOAT4, 0, D3DDECLUSAGE_BLENDWEIGHT, 0},
            {0, 28, D3DDECLTYPE_UBYTE4, 0, D3DDECLUSAGE_BLENDINDICES, 0},
            D3DDECL_END(),
        }, D3DFVF_XYZB5 | D3DFVF_LASTBETA_UBYTE4},
        {{
            {0, 0, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_POSITION, 0},
            {0, 12, D3DDECLTYPE_FLOAT4, 0, D3DDECLUSAGE_BLENDWEIGHT, 0},
            {0, 28, D3DDECLTYPE_D3DCOLOR, 0, D3DDECLUSAGE_BLENDINDICES, 0},
            D3DDECL_END(),
        }, D3DFVF_XYZB5 | D3DFVF_LASTBETA_D3DCOLOR},
        {{
            {0, 0, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_NORMAL, 0},
            D3DDECL_END(),
        }, D3DFVF_NORMAL},
        {{
            {0, 0, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_NORMAL, 0},
            {0, 12, D3DDECLTYPE_D3DCOLOR, 0, D3DDECLUSAGE_COLOR, 0},
            D3DDECL_END(),
        }, D3DFVF_NORMAL | D3DFVF_DIFFUSE},
        {{
            {0, 0, D3DDECLTYPE_FLOAT1, 0, D3DDECLUSAGE_PSIZE, 0},
            D3DDECL_END(),
        }, D3DFVF_PSIZE},
        {{
            {0, 0, D3DDECLTYPE_D3DCOLOR, 0, D3DDECLUSAGE_COLOR, 0},
            D3DDECL_END(),
        }, D3DFVF_DIFFUSE},
        {{
            {0, 0, D3DDECLTYPE_D3DCOLOR, 0, D3DDECLUSAGE_COLOR, 1},
            D3DDECL_END(),
        }, D3DFVF_SPECULAR},
        /* Make sure textures of different sizes work. */
        {{
            {0, 0, D3DDECLTYPE_FLOAT1, 0, D3DDECLUSAGE_TEXCOORD, 0},
            D3DDECL_END(),
        }, D3DFVF_TEXCOORDSIZE1(0) | D3DFVF_TEX1},
        {{
            {0, 0, D3DDECLTYPE_FLOAT2, 0, D3DDECLUSAGE_TEXCOORD, 0},
            D3DDECL_END(),
        }, D3DFVF_TEXCOORDSIZE2(0) | D3DFVF_TEX1},
        {{
            {0, 0, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_TEXCOORD, 0},
            D3DDECL_END(),
        }, D3DFVF_TEXCOORDSIZE3(0) | D3DFVF_TEX1},
        {{
            {0, 0, D3DDECLTYPE_FLOAT4, 0, D3DDECLUSAGE_TEXCOORD, 0},
            D3DDECL_END(),
        }, D3DFVF_TEXCOORDSIZE4(0) | D3DFVF_TEX1},
        /* Make sure the TEXCOORD index works correctly - try several textures. */
        {{
            {0, 0, D3DDECLTYPE_FLOAT1, 0, D3DDECLUSAGE_TEXCOORD, 0},
            {0, 4, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_TEXCOORD, 1},
            {0, 16, D3DDECLTYPE_FLOAT2, 0, D3DDECLUSAGE_TEXCOORD, 2},
            {0, 24, D3DDECLTYPE_FLOAT4, 0, D3DDECLUSAGE_TEXCOORD, 3},
            D3DDECL_END(),
        }, D3DFVF_TEX4 | D3DFVF_TEXCOORDSIZE1(0) | D3DFVF_TEXCOORDSIZE3(1)
                | D3DFVF_TEXCOORDSIZE2(2) | D3DFVF_TEXCOORDSIZE4(3)},
        /* Now try some combination tests. */
        {{
            {0, 0, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_POSITION, 0},
            {0, 12, D3DDECLTYPE_FLOAT4, 0, D3DDECLUSAGE_BLENDWEIGHT, 0},
            {0, 28, D3DDECLTYPE_D3DCOLOR, 0, D3DDECLUSAGE_COLOR, 0},
            {0, 32, D3DDECLTYPE_D3DCOLOR, 0, D3DDECLUSAGE_COLOR, 1},
            {0, 36, D3DDECLTYPE_FLOAT2, 0, D3DDECLUSAGE_TEXCOORD, 0},
            {0, 44, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_TEXCOORD, 1},
            D3DDECL_END(),
        }, D3DFVF_XYZB4 | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX2
                | D3DFVF_TEXCOORDSIZE2(0) | D3DFVF_TEXCOORDSIZE3(1)},
        {{
            {0, 0, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_POSITION, 0},
            {0, 12, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_NORMAL, 0},
            {0, 24, D3DDECLTYPE_FLOAT1, 0, D3DDECLUSAGE_PSIZE, 0},
            {0, 28, D3DDECLTYPE_D3DCOLOR, 0, D3DDECLUSAGE_COLOR, 1},
            {0, 32, D3DDECLTYPE_FLOAT1, 0, D3DDECLUSAGE_TEXCOORD, 0},
            {0, 36, D3DDECLTYPE_FLOAT4, 0, D3DDECLUSAGE_TEXCOORD, 1},
            D3DDECL_END(),
        }, D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_PSIZE | D3DFVF_SPECULAR | D3DFVF_TEX2
                | D3DFVF_TEXCOORDSIZE1(0) | D3DFVF_TEXCOORDSIZE4(1)},
    };
    unsigned int i;

    for (i = 0; i < sizeof(test_data) / sizeof(*test_data); ++i)
    {
        test_decl_to_fvf(test_data[i].decl, test_data[i].fvf, D3D_OK, __LINE__, i);
        test_fvf_to_decl(test_data[i].fvf, test_data[i].decl, D3D_OK, __LINE__, i);
    }

    /* Usage indices for position and normal are apparently ignored. */
    {
        const D3DVERTEXELEMENT9 decl[] =
        {
            {0, 0, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_POSITION, 1},
            D3DDECL_END(),
        };
        test_decl_to_fvf(decl, D3DFVF_XYZ, D3D_OK, __LINE__, 0);
    }
    {
        const D3DVERTEXELEMENT9 decl[] =
        {
            {0, 0, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_NORMAL, 1},
            D3DDECL_END(),
        };
        test_decl_to_fvf(decl, D3DFVF_NORMAL, D3D_OK, __LINE__, 0);
    }
    /* D3DFVF_LASTBETA_UBYTE4 and D3DFVF_LASTBETA_D3DCOLOR are ignored if
     * there are no blend matrices. */
    {
        const D3DVERTEXELEMENT9 decl[] =
        {
            {0, 0, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_POSITION, 0},
            D3DDECL_END(),
        };
        test_fvf_to_decl(D3DFVF_XYZ | D3DFVF_LASTBETA_UBYTE4, decl, D3D_OK, __LINE__, 0);
    }
    {
        const D3DVERTEXELEMENT9 decl[] =
        {
            {0, 0, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_POSITION, 0},
            D3DDECL_END(),
        };
        test_fvf_to_decl(D3DFVF_XYZ | D3DFVF_LASTBETA_D3DCOLOR, decl, D3D_OK, __LINE__, 0);
    }
    /* D3DFVF_LASTBETA_UBYTE4 takes precedence over D3DFVF_LASTBETA_D3DCOLOR. */
    {
        const D3DVERTEXELEMENT9 decl[] =
        {
            {0, 0, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_POSITION, 0},
            {0, 12, D3DDECLTYPE_FLOAT4, 0, D3DDECLUSAGE_BLENDWEIGHT, 0},
            {0, 28, D3DDECLTYPE_UBYTE4, 0, D3DDECLUSAGE_BLENDINDICES, 0},
            D3DDECL_END(),
        };
        test_fvf_to_decl(D3DFVF_XYZB5 | D3DFVF_LASTBETA_D3DCOLOR | D3DFVF_LASTBETA_UBYTE4,
                decl, D3D_OK, __LINE__, 0);
    }
    /* These are supposed to fail, both ways. */
    {
        const D3DVERTEXELEMENT9 decl[] =
        {
            {0, 0, D3DDECLTYPE_FLOAT4, 0, D3DDECLUSAGE_POSITION, 0},
            D3DDECL_END(),
        };
        test_decl_to_fvf(decl, D3DFVF_XYZW, D3DERR_INVALIDCALL, __LINE__, 0);
        test_fvf_to_decl(D3DFVF_XYZW, decl, D3DERR_INVALIDCALL, __LINE__, 0);
    }
    {
        const D3DVERTEXELEMENT9 decl[] =
        {
            {0, 0, D3DDECLTYPE_FLOAT4, 0, D3DDECLUSAGE_POSITION, 0},
            {0, 16, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_NORMAL, 0},
            D3DDECL_END(),
        };
        test_decl_to_fvf(decl, D3DFVF_XYZW | D3DFVF_NORMAL, D3DERR_INVALIDCALL, __LINE__, 0);
        test_fvf_to_decl(D3DFVF_XYZW | D3DFVF_NORMAL, decl, D3DERR_INVALIDCALL, __LINE__, 0);
    }
    {
        const D3DVERTEXELEMENT9 decl[] =
        {
            {0, 0, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_POSITION, 0},
            {0, 12, D3DDECLTYPE_FLOAT4, 0, D3DDECLUSAGE_BLENDWEIGHT, 0},
            {0, 28, D3DDECLTYPE_FLOAT1, 0, D3DDECLUSAGE_BLENDINDICES, 0},
            D3DDECL_END(),
        };
        test_decl_to_fvf(decl, D3DFVF_XYZB5, D3DERR_INVALIDCALL, __LINE__, 0);
        test_fvf_to_decl(D3DFVF_XYZB5, decl, D3DERR_INVALIDCALL, __LINE__, 0);
    }
    /* Test a declaration that can't be converted to an FVF. */
    {
        const D3DVERTEXELEMENT9 decl[] =
        {
            {0, 0, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_POSITION, 0},
            {0, 12, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_NORMAL, 0},
            {0, 24, D3DDECLTYPE_FLOAT1, 0, D3DDECLUSAGE_PSIZE, 0},
            {0, 28, D3DDECLTYPE_D3DCOLOR, 0, D3DDECLUSAGE_COLOR, 1},
            {0, 32, D3DDECLTYPE_FLOAT1, 0, D3DDECLUSAGE_TEXCOORD, 0},
            /* 8 bytes padding */
            {0, 44, D3DDECLTYPE_FLOAT4, 0, D3DDECLUSAGE_TEXCOORD, 1},
            D3DDECL_END(),
        };
        test_decl_to_fvf(decl, 0, D3DERR_INVALIDCALL, __LINE__, 0);
    }
    /* Elements must be ordered by offset. */
    {
        const D3DVERTEXELEMENT9 decl[] =
        {
            {0, 12, D3DDECLTYPE_D3DCOLOR, 0, D3DDECLUSAGE_COLOR, 0},
            {0, 0, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_POSITION, 0},
            D3DDECL_END(),
        };
        test_decl_to_fvf(decl, 0, D3DERR_INVALIDCALL, __LINE__, 0);
    }
    /* Basic tests for element order. */
    {
        const D3DVERTEXELEMENT9 decl[] =
        {
            {0, 0, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_POSITION, 0},
            {0, 12, D3DDECLTYPE_D3DCOLOR, 0, D3DDECLUSAGE_COLOR, 0},
            {0, 16, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_NORMAL, 0},
            D3DDECL_END(),
        };
        test_decl_to_fvf(decl, 0, D3DERR_INVALIDCALL, __LINE__, 0);
    }
    {
        const D3DVERTEXELEMENT9 decl[] =
        {
            {0, 0, D3DDECLTYPE_D3DCOLOR, 0, D3DDECLUSAGE_COLOR, 0},
            {0, 4, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_POSITION, 0},
            D3DDECL_END(),
        };
        test_decl_to_fvf(decl, 0, D3DERR_INVALIDCALL, __LINE__, 0);
    }
    {
        const D3DVERTEXELEMENT9 decl[] =
        {
            {0, 0, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_NORMAL, 0},
            {0, 12, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_POSITION, 0},
            D3DDECL_END(),
        };
        test_decl_to_fvf(decl, 0, D3DERR_INVALIDCALL, __LINE__, 0);
    }
    /* Textures must be ordered by texcoords. */
    {
        const D3DVERTEXELEMENT9 decl[] =
        {
            {0, 0, D3DDECLTYPE_FLOAT1, 0, D3DDECLUSAGE_TEXCOORD, 0},
            {0, 4, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_TEXCOORD, 2},
            {0, 16, D3DDECLTYPE_FLOAT2, 0, D3DDECLUSAGE_TEXCOORD, 1},
            {0, 24, D3DDECLTYPE_FLOAT4, 0, D3DDECLUSAGE_TEXCOORD, 3},
            D3DDECL_END(),
        };
        test_decl_to_fvf(decl, 0, D3DERR_INVALIDCALL, __LINE__, 0);
    }
    /* Duplicate elements are not allowed. */
    {
        const D3DVERTEXELEMENT9 decl[] =
        {
            {0, 0, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_POSITION, 0},
            {0, 12, D3DDECLTYPE_D3DCOLOR, 0, D3DDECLUSAGE_COLOR, 0},
            {0, 16, D3DDECLTYPE_D3DCOLOR, 0, D3DDECLUSAGE_COLOR, 0},
            D3DDECL_END(),
        };
        test_decl_to_fvf(decl, 0, D3DERR_INVALIDCALL, __LINE__, 0);
    }
    /* Invalid FVFs cannot be converted to a declarator. */
    test_fvf_to_decl(0xdeadbeef, NULL, D3DERR_INVALIDCALL, __LINE__, 0);
}

static void D3DXGetFVFVertexSizeTest(void)
{
    UINT got;

    compare_vertex_sizes (D3DFVF_XYZ, 12);

    compare_vertex_sizes (D3DFVF_XYZB3, 24);

    compare_vertex_sizes (D3DFVF_XYZB5, 32);

    compare_vertex_sizes (D3DFVF_XYZ | D3DFVF_NORMAL, 24);

    compare_vertex_sizes (D3DFVF_XYZ | D3DFVF_DIFFUSE, 16);

    compare_vertex_sizes (
        D3DFVF_XYZ |
        D3DFVF_TEX1 |
        D3DFVF_TEXCOORDSIZE1(0), 16);
    compare_vertex_sizes (
        D3DFVF_XYZ |
        D3DFVF_TEX2 |
        D3DFVF_TEXCOORDSIZE1(0) |
        D3DFVF_TEXCOORDSIZE1(1), 20);

    compare_vertex_sizes (
        D3DFVF_XYZ |
        D3DFVF_TEX1 |
        D3DFVF_TEXCOORDSIZE2(0), 20);

    compare_vertex_sizes (
        D3DFVF_XYZ |
        D3DFVF_TEX2 |
        D3DFVF_TEXCOORDSIZE2(0) |
        D3DFVF_TEXCOORDSIZE2(1), 28);

    compare_vertex_sizes (
        D3DFVF_XYZ |
        D3DFVF_TEX6 |
        D3DFVF_TEXCOORDSIZE2(0) |
        D3DFVF_TEXCOORDSIZE2(1) |
        D3DFVF_TEXCOORDSIZE2(2) |
        D3DFVF_TEXCOORDSIZE2(3) |
        D3DFVF_TEXCOORDSIZE2(4) |
        D3DFVF_TEXCOORDSIZE2(5), 60);

    compare_vertex_sizes (
        D3DFVF_XYZ |
        D3DFVF_TEX8 |
        D3DFVF_TEXCOORDSIZE2(0) |
        D3DFVF_TEXCOORDSIZE2(1) |
        D3DFVF_TEXCOORDSIZE2(2) |
        D3DFVF_TEXCOORDSIZE2(3) |
        D3DFVF_TEXCOORDSIZE2(4) |
        D3DFVF_TEXCOORDSIZE2(5) |
        D3DFVF_TEXCOORDSIZE2(6) |
        D3DFVF_TEXCOORDSIZE2(7), 76);

    compare_vertex_sizes (
        D3DFVF_XYZ |
        D3DFVF_TEX1 |
        D3DFVF_TEXCOORDSIZE3(0), 24);

    compare_vertex_sizes (
        D3DFVF_XYZ |
        D3DFVF_TEX4 |
        D3DFVF_TEXCOORDSIZE3(0) |
        D3DFVF_TEXCOORDSIZE3(1) |
        D3DFVF_TEXCOORDSIZE3(2) |
        D3DFVF_TEXCOORDSIZE3(3), 60);

    compare_vertex_sizes (
        D3DFVF_XYZ |
        D3DFVF_TEX1 |
        D3DFVF_TEXCOORDSIZE4(0), 28);

    compare_vertex_sizes (
        D3DFVF_XYZ |
        D3DFVF_TEX2 |
        D3DFVF_TEXCOORDSIZE4(0) |
        D3DFVF_TEXCOORDSIZE4(1), 44);

    compare_vertex_sizes (
        D3DFVF_XYZ |
        D3DFVF_TEX3 |
        D3DFVF_TEXCOORDSIZE4(0) |
        D3DFVF_TEXCOORDSIZE4(1) |
        D3DFVF_TEXCOORDSIZE4(2), 60);

    compare_vertex_sizes (
        D3DFVF_XYZB5 |
        D3DFVF_NORMAL |
        D3DFVF_DIFFUSE |
        D3DFVF_SPECULAR |
        D3DFVF_TEX8 |
        D3DFVF_TEXCOORDSIZE4(0) |
        D3DFVF_TEXCOORDSIZE4(1) |
        D3DFVF_TEXCOORDSIZE4(2) |
        D3DFVF_TEXCOORDSIZE4(3) |
        D3DFVF_TEXCOORDSIZE4(4) |
        D3DFVF_TEXCOORDSIZE4(5) |
        D3DFVF_TEXCOORDSIZE4(6) |
        D3DFVF_TEXCOORDSIZE4(7), 180);
}

static void D3DXIntersectTriTest(void)
{
    BOOL exp_res, got_res;
    D3DXVECTOR3 position, ray, vertex[3];
    FLOAT exp_dist, got_dist, exp_u, got_u, exp_v, got_v;

    vertex[0].x = 1.0f; vertex[0].y = 0.0f; vertex[0].z = 0.0f;
    vertex[1].x = 2.0f; vertex[1].y = 0.0f; vertex[1].z = 0.0f;
    vertex[2].x = 1.0f; vertex[2].y = 1.0f; vertex[2].z = 0.0f;

    position.x = -14.5f; position.y = -23.75f; position.z = -32.0f;

    ray.x = 2.0f; ray.y = 3.0f; ray.z = 4.0f;

    exp_res = TRUE; exp_u = 0.5f; exp_v = 0.25f; exp_dist = 8.0f;

    got_res = D3DXIntersectTri(&vertex[0],&vertex[1],&vertex[2],&position,&ray,&got_u,&got_v,&got_dist);
    ok( got_res == exp_res, "Expected result = %d, got %d\n",exp_res,got_res);
    ok( compare(exp_u,got_u), "Expected u = %f, got %f\n",exp_u,got_u);
    ok( compare(exp_v,got_v), "Expected v = %f, got %f\n",exp_v,got_v);
    ok( compare(exp_dist,got_dist), "Expected distance = %f, got %f\n",exp_dist,got_dist);

/*Only positive ray is taken in account*/

    vertex[0].x = 1.0f; vertex[0].y = 0.0f; vertex[0].z = 0.0f;
    vertex[1].x = 2.0f; vertex[1].y = 0.0f; vertex[1].z = 0.0f;
    vertex[2].x = 1.0f; vertex[2].y = 1.0f; vertex[2].z = 0.0f;

    position.x = 17.5f; position.y = 24.25f; position.z = 32.0f;

    ray.x = 2.0f; ray.y = 3.0f; ray.z = 4.0f;

    exp_res = FALSE;

    got_res = D3DXIntersectTri(&vertex[0],&vertex[1],&vertex[2],&position,&ray,&got_u,&got_v,&got_dist);
    ok( got_res == exp_res, "Expected result = %d, got %d\n",exp_res,got_res);

/*Intersection between ray and triangle in a same plane is considered as empty*/

    vertex[0].x = 4.0f; vertex[0].y = 0.0f; vertex[0].z = 0.0f;
    vertex[1].x = 6.0f; vertex[1].y = 0.0f; vertex[1].z = 0.0f;
    vertex[2].x = 4.0f; vertex[2].y = 2.0f; vertex[2].z = 0.0f;

    position.x = 1.0f; position.y = 1.0f; position.z = 0.0f;

    ray.x = 1.0f; ray.y = 0.0f; ray.z = 0.0f;

    exp_res = FALSE;

    got_res = D3DXIntersectTri(&vertex[0],&vertex[1],&vertex[2],&position,&ray,&got_u,&got_v,&got_dist);
    ok( got_res == exp_res, "Expected result = %d, got %d\n",exp_res,got_res);
}

static void D3DXCreateMeshTest(void)
{
    HRESULT hr;
    HWND wnd;
    IDirect3D9 *d3d;
    IDirect3DDevice9 *device, *test_device;
    D3DPRESENT_PARAMETERS d3dpp;
    ID3DXMesh *d3dxmesh;
    int i, size;
    D3DVERTEXELEMENT9 test_decl[MAX_FVF_DECL_SIZE];
    DWORD options;
    struct mesh mesh;

    static const D3DVERTEXELEMENT9 decl1[3] = {
        {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0},
        D3DDECL_END(), };

    static const D3DVERTEXELEMENT9 decl2[] = {
        {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0},
        {0, 24, D3DDECLTYPE_FLOAT1, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_PSIZE, 0},
        {0, 28, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 1},
        {0, 32, D3DDECLTYPE_FLOAT1, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
        /* 8 bytes padding */
        {0, 44, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1},
        D3DDECL_END(),
    };

    static const D3DVERTEXELEMENT9 decl3[] = {
        {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {1, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0},
        D3DDECL_END(),
    };

    hr = D3DXCreateMesh(0, 0, 0, NULL, NULL, NULL);
    ok(hr == D3DERR_INVALIDCALL, "Got result %x, expected %x (D3DERR_INVALIDCALL)\n", hr, D3DERR_INVALIDCALL);

    hr = D3DXCreateMesh(1, 3, D3DXMESH_MANAGED, decl1, NULL, &d3dxmesh);
    ok(hr == D3DERR_INVALIDCALL, "Got result %x, expected %x (D3DERR_INVALIDCALL)\n", hr, D3DERR_INVALIDCALL);

    wnd = CreateWindow("static", "d3dx9_test", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL);
    if (!wnd)
    {
        skip("Couldn't create application window\n");
        return;
    }
    d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
    {
        skip("Couldn't create IDirect3D9 object\n");
        DestroyWindow(wnd);
        return;
    }

    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    hr = IDirect3D9_CreateDevice(d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, wnd, D3DCREATE_MIXED_VERTEXPROCESSING, &d3dpp, &device);
    if (FAILED(hr))
    {
        skip("Failed to create IDirect3DDevice9 object %#x\n", hr);
        IDirect3D9_Release(d3d);
        DestroyWindow(wnd);
        return;
    }

    hr = D3DXCreateMesh(0, 3, D3DXMESH_MANAGED, decl1, device, &d3dxmesh);
    ok(hr == D3DERR_INVALIDCALL, "Got result %x, expected %x (D3DERR_INVALIDCALL)\n", hr, D3DERR_INVALIDCALL);

    hr = D3DXCreateMesh(1, 0, D3DXMESH_MANAGED, decl1, device, &d3dxmesh);
    ok(hr == D3DERR_INVALIDCALL, "Got result %x, expected %x (D3DERR_INVALIDCALL)\n", hr, D3DERR_INVALIDCALL);

    hr = D3DXCreateMesh(1, 3, 0, decl1, device, &d3dxmesh);
    ok(hr == D3D_OK, "Got result %x, expected %x (D3D_OK)\n", hr, D3D_OK);

    if (hr == D3D_OK)
    {
        d3dxmesh->lpVtbl->Release(d3dxmesh);
    }

    hr = D3DXCreateMesh(1, 3, D3DXMESH_MANAGED, 0, device, &d3dxmesh);
    ok(hr == D3DERR_INVALIDCALL, "Got result %x, expected %x (D3DERR_INVALIDCALL)\n", hr, D3DERR_INVALIDCALL);

    hr = D3DXCreateMesh(1, 3, D3DXMESH_MANAGED, decl1, device, NULL);
    ok(hr == D3DERR_INVALIDCALL, "Got result %x, expected %x (D3DERR_INVALIDCALL)\n", hr, D3DERR_INVALIDCALL);

    hr = D3DXCreateMesh(1, 3, D3DXMESH_MANAGED, decl1, device, &d3dxmesh);
    ok(hr == D3D_OK, "Got result %x, expected 0 (D3D_OK)\n", hr);

    if (hr == D3D_OK)
    {
        /* device */
        hr = d3dxmesh->lpVtbl->GetDevice(d3dxmesh, NULL);
        ok(hr == D3DERR_INVALIDCALL, "Got result %x, expected %x (D3DERR_INVALIDCALL)\n", hr, D3DERR_INVALIDCALL);

        hr = d3dxmesh->lpVtbl->GetDevice(d3dxmesh, &test_device);
        ok(hr == D3D_OK, "Got result %x, expected %x (D3D_OK)\n", hr, D3D_OK);
        ok(test_device == device, "Got result %p, expected %p\n", test_device, device);

        if (hr == D3D_OK)
        {
            IDirect3DDevice9_Release(device);
        }

        /* declaration */
        hr = d3dxmesh->lpVtbl->GetDeclaration(d3dxmesh, NULL);
        ok(hr == D3DERR_INVALIDCALL, "Got result %x, expected %x (D3DERR_INVALIDCALL)\n", hr, D3DERR_INVALIDCALL);

        hr = d3dxmesh->lpVtbl->GetDeclaration(d3dxmesh, test_decl);
        ok(hr == D3D_OK, "Got result %x, expected 0 (D3D_OK)\n", hr);

        if (hr == D3D_OK)
        {
            size = sizeof(decl1) / sizeof(decl1[0]);
            for (i = 0; i < size - 1; i++)
            {
                ok(test_decl[i].Stream == decl1[i].Stream, "Returned stream %d, expected %d\n", test_decl[i].Stream, decl1[i].Stream);
                ok(test_decl[i].Type == decl1[i].Type, "Returned type %d, expected %d\n", test_decl[i].Type, decl1[i].Type);
                ok(test_decl[i].Method == decl1[i].Method, "Returned method %d, expected %d\n", test_decl[i].Method, decl1[i].Method);
                ok(test_decl[i].Usage == decl1[i].Usage, "Returned usage %d, expected %d\n", test_decl[i].Usage, decl1[i].Usage);
                ok(test_decl[i].UsageIndex == decl1[i].UsageIndex, "Returned usage index %d, expected %d\n", test_decl[i].UsageIndex, decl1[i].UsageIndex);
                ok(test_decl[i].Offset == decl1[i].Offset, "Returned offset %d, expected %d\n", test_decl[i].Offset, decl1[i].Offset);
            }
            ok(decl1[size-1].Stream == 0xFF, "Returned too long vertex declaration\n"); /* end element */
        }

        /* options */
        options = d3dxmesh->lpVtbl->GetOptions(d3dxmesh);
        ok(options == D3DXMESH_MANAGED, "Got result %x, expected %x (D3DXMESH_MANAGED)\n", options, D3DXMESH_MANAGED);

        /* rest */
        if (!new_mesh(&mesh, 3, 1))
        {
            skip("Couldn't create mesh\n");
        }
        else
        {
            memset(mesh.vertices, 0, mesh.number_of_vertices * sizeof(*mesh.vertices));
            memset(mesh.faces, 0, mesh.number_of_faces * sizeof(*mesh.faces));
            mesh.fvf = D3DFVF_XYZ | D3DFVF_NORMAL;

            compare_mesh("createmesh1", d3dxmesh, &mesh);

            free_mesh(&mesh);
        }

        d3dxmesh->lpVtbl->Release(d3dxmesh);
    }

    /* Test a declaration that can't be converted to an FVF. */
    hr = D3DXCreateMesh(1, 3, D3DXMESH_MANAGED, decl2, device, &d3dxmesh);
    ok(hr == D3D_OK, "Got result %x, expected 0 (D3D_OK)\n", hr);

    if (hr == D3D_OK)
    {
        /* device */
        hr = d3dxmesh->lpVtbl->GetDevice(d3dxmesh, NULL);
        ok(hr == D3DERR_INVALIDCALL, "Got result %x, expected %x (D3DERR_INVALIDCALL)\n", hr, D3DERR_INVALIDCALL);

        hr = d3dxmesh->lpVtbl->GetDevice(d3dxmesh, &test_device);
        ok(hr == D3D_OK, "Got result %x, expected %x (D3D_OK)\n", hr, D3D_OK);
        ok(test_device == device, "Got result %p, expected %p\n", test_device, device);

        if (hr == D3D_OK)
        {
            IDirect3DDevice9_Release(device);
        }

        /* declaration */
        hr = d3dxmesh->lpVtbl->GetDeclaration(d3dxmesh, test_decl);
        ok(hr == D3D_OK, "Got result %x, expected 0 (D3D_OK)\n", hr);

        if (hr == D3D_OK)
        {
            size = sizeof(decl2) / sizeof(decl2[0]);
            for (i = 0; i < size - 1; i++)
            {
                ok(test_decl[i].Stream == decl2[i].Stream, "Returned stream %d, expected %d\n", test_decl[i].Stream, decl2[i].Stream);
                ok(test_decl[i].Type == decl2[i].Type, "Returned type %d, expected %d\n", test_decl[i].Type, decl2[i].Type);
                ok(test_decl[i].Method == decl2[i].Method, "Returned method %d, expected %d\n", test_decl[i].Method, decl2[i].Method);
                ok(test_decl[i].Usage == decl2[i].Usage, "Returned usage %d, expected %d\n", test_decl[i].Usage, decl2[i].Usage);
                ok(test_decl[i].UsageIndex == decl2[i].UsageIndex, "Returned usage index %d, expected %d\n", test_decl[i].UsageIndex, decl2[i].UsageIndex);
                ok(test_decl[i].Offset == decl2[i].Offset, "Returned offset %d, expected %d\n", test_decl[i].Offset, decl2[i].Offset);
            }
            ok(decl2[size-1].Stream == 0xFF, "Returned too long vertex declaration\n"); /* end element */
        }

        /* options */
        options = d3dxmesh->lpVtbl->GetOptions(d3dxmesh);
        ok(options == D3DXMESH_MANAGED, "Got result %x, expected %x (D3DXMESH_MANAGED)\n", options, D3DXMESH_MANAGED);

        /* rest */
        if (!new_mesh(&mesh, 3, 1))
        {
            skip("Couldn't create mesh\n");
        }
        else
        {
            memset(mesh.vertices, 0, mesh.number_of_vertices * sizeof(*mesh.vertices));
            memset(mesh.faces, 0, mesh.number_of_faces * sizeof(*mesh.faces));
            mesh.fvf = 0;
            mesh.vertex_size = 60;

            compare_mesh("createmesh2", d3dxmesh, &mesh);

            free_mesh(&mesh);
        }

        mesh.vertex_size = d3dxmesh->lpVtbl->GetNumBytesPerVertex(d3dxmesh);
        ok(mesh.vertex_size == 60, "Got vertex size %u, expected %u\n", mesh.vertex_size, 60);

        d3dxmesh->lpVtbl->Release(d3dxmesh);
    }

    /* Test a declaration with multiple streams. */
    hr = D3DXCreateMesh(1, 3, D3DXMESH_MANAGED, decl3, device, &d3dxmesh);
    ok(hr == D3DERR_INVALIDCALL, "Got result %x, expected %x (D3DERR_INVALIDCALL)\n", hr, D3DERR_INVALIDCALL);

    IDirect3DDevice9_Release(device);
    IDirect3D9_Release(d3d);
    DestroyWindow(wnd);
}

static void D3DXCreateMeshFVFTest(void)
{
    HRESULT hr;
    HWND wnd;
    IDirect3D9 *d3d;
    IDirect3DDevice9 *device, *test_device;
    D3DPRESENT_PARAMETERS d3dpp;
    ID3DXMesh *d3dxmesh;
    int i, size;
    D3DVERTEXELEMENT9 test_decl[MAX_FVF_DECL_SIZE];
    DWORD options;
    struct mesh mesh;

    static const D3DVERTEXELEMENT9 decl[3] = {
        {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0},
        D3DDECL_END(), };

    hr = D3DXCreateMeshFVF(0, 0, 0, 0, NULL, NULL);
    ok(hr == D3DERR_INVALIDCALL, "Got result %x, expected %x (D3DERR_INVALIDCALL)\n", hr, D3DERR_INVALIDCALL);

    hr = D3DXCreateMeshFVF(1, 3, D3DXMESH_MANAGED, D3DFVF_XYZ | D3DFVF_NORMAL, NULL, &d3dxmesh);
    ok(hr == D3DERR_INVALIDCALL, "Got result %x, expected %x (D3DERR_INVALIDCALL)\n", hr, D3DERR_INVALIDCALL);

    wnd = CreateWindow("static", "d3dx9_test", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL);
    if (!wnd)
    {
        skip("Couldn't create application window\n");
        return;
    }
    d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
    {
        skip("Couldn't create IDirect3D9 object\n");
        DestroyWindow(wnd);
        return;
    }

    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    hr = IDirect3D9_CreateDevice(d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, wnd, D3DCREATE_MIXED_VERTEXPROCESSING, &d3dpp, &device);
    if (FAILED(hr))
    {
        skip("Failed to create IDirect3DDevice9 object %#x\n", hr);
        IDirect3D9_Release(d3d);
        DestroyWindow(wnd);
        return;
    }

    hr = D3DXCreateMeshFVF(0, 3, D3DXMESH_MANAGED, D3DFVF_XYZ | D3DFVF_NORMAL, device, &d3dxmesh);
    ok(hr == D3DERR_INVALIDCALL, "Got result %x, expected %x (D3DERR_INVALIDCALL)\n", hr, D3DERR_INVALIDCALL);

    hr = D3DXCreateMeshFVF(1, 0, D3DXMESH_MANAGED, D3DFVF_XYZ | D3DFVF_NORMAL, device, &d3dxmesh);
    ok(hr == D3DERR_INVALIDCALL, "Got result %x, expected %x (D3DERR_INVALIDCALL)\n", hr, D3DERR_INVALIDCALL);

    hr = D3DXCreateMeshFVF(1, 3, 0, D3DFVF_XYZ | D3DFVF_NORMAL, device, &d3dxmesh);
    ok(hr == D3D_OK, "Got result %x, expected %x (D3D_OK)\n", hr, D3D_OK);

    if (hr == D3D_OK)
    {
        d3dxmesh->lpVtbl->Release(d3dxmesh);
    }

    hr = D3DXCreateMeshFVF(1, 3, D3DXMESH_MANAGED, 0xdeadbeef, device, &d3dxmesh);
    ok(hr == D3DERR_INVALIDCALL, "Got result %x, expected %x (D3DERR_INVALIDCALL)\n", hr, D3DERR_INVALIDCALL);

    hr = D3DXCreateMeshFVF(1, 3, D3DXMESH_MANAGED, D3DFVF_XYZ | D3DFVF_NORMAL, device, NULL);
    ok(hr == D3DERR_INVALIDCALL, "Got result %x, expected %x (D3DERR_INVALIDCALL)\n", hr, D3DERR_INVALIDCALL);

    hr = D3DXCreateMeshFVF(1, 3, D3DXMESH_MANAGED, D3DFVF_XYZ | D3DFVF_NORMAL, device, &d3dxmesh);
    ok(hr == D3D_OK, "Got result %x, expected 0 (D3D_OK)\n", hr);

    if (hr == D3D_OK)
    {
        /* device */
        hr = d3dxmesh->lpVtbl->GetDevice(d3dxmesh, NULL);
        ok(hr == D3DERR_INVALIDCALL, "Got result %x, expected %x (D3DERR_INVALIDCALL)\n", hr, D3DERR_INVALIDCALL);

        hr = d3dxmesh->lpVtbl->GetDevice(d3dxmesh, &test_device);
        ok(hr == D3D_OK, "Got result %x, expected %x (D3D_OK)\n", hr, D3D_OK);
        ok(test_device == device, "Got result %p, expected %p\n", test_device, device);

        if (hr == D3D_OK)
        {
            IDirect3DDevice9_Release(device);
        }

        /* declaration */
        hr = d3dxmesh->lpVtbl->GetDeclaration(d3dxmesh, NULL);
        ok(hr == D3DERR_INVALIDCALL, "Got result %x, expected %x (D3DERR_INVALIDCALL)\n", hr, D3DERR_INVALIDCALL);

        hr = d3dxmesh->lpVtbl->GetDeclaration(d3dxmesh, test_decl);
        ok(hr == D3D_OK, "Got result %x, expected 0 (D3D_OK)\n", hr);

        if (hr == D3D_OK)
        {
            size = sizeof(decl) / sizeof(decl[0]);
            for (i = 0; i < size - 1; i++)
            {
                ok(test_decl[i].Stream == decl[i].Stream, "Returned stream %d, expected %d\n", test_decl[i].Stream, decl[i].Stream);
                ok(test_decl[i].Type == decl[i].Type, "Returned type %d, expected %d\n", test_decl[i].Type, decl[i].Type);
                ok(test_decl[i].Method == decl[i].Method, "Returned method %d, expected %d\n", test_decl[i].Method, decl[i].Method);
                ok(test_decl[i].Usage == decl[i].Usage, "Returned usage %d, expected %d\n", test_decl[i].Usage, decl[i].Usage);
                ok(test_decl[i].UsageIndex == decl[i].UsageIndex, "Returned usage index %d, expected %d\n",
                   test_decl[i].UsageIndex, decl[i].UsageIndex);
                ok(test_decl[i].Offset == decl[i].Offset, "Returned offset %d, expected %d\n", test_decl[i].Offset, decl[i].Offset);
            }
            ok(decl[size-1].Stream == 0xFF, "Returned too long vertex declaration\n"); /* end element */
        }

        /* options */
        options = d3dxmesh->lpVtbl->GetOptions(d3dxmesh);
        ok(options == D3DXMESH_MANAGED, "Got result %x, expected %x (D3DXMESH_MANAGED)\n", options, D3DXMESH_MANAGED);

        /* rest */
        if (!new_mesh(&mesh, 3, 1))
        {
            skip("Couldn't create mesh\n");
        }
        else
        {
            memset(mesh.vertices, 0, mesh.number_of_vertices * sizeof(*mesh.vertices));
            memset(mesh.faces, 0, mesh.number_of_faces * sizeof(*mesh.faces));
            mesh.fvf = D3DFVF_XYZ | D3DFVF_NORMAL;

            compare_mesh("createmeshfvf", d3dxmesh, &mesh);

            free_mesh(&mesh);
        }

        d3dxmesh->lpVtbl->Release(d3dxmesh);
    }

    IDirect3DDevice9_Release(device);
    IDirect3D9_Release(d3d);
    DestroyWindow(wnd);
}

static void D3DXCreateBoxTest(void)
{
    HRESULT hr;
    HWND wnd;
    WNDCLASS wc={0};
    IDirect3D9* d3d;
    IDirect3DDevice9* device;
    D3DPRESENT_PARAMETERS d3dpp;
    ID3DXMesh* box;
    ID3DXBuffer* ppBuffer;
    DWORD *buffer;
    static const DWORD adjacency[36]=
        {6, 9, 1, 2, 10, 0,
         1, 9, 3, 4, 10, 2,
         3, 8, 5, 7, 11, 4,
         0, 11, 7, 5, 8, 6,
         7, 4, 9, 2, 0, 8,
         1, 3, 11, 5, 6, 10};
    unsigned int i;

    wc.lpfnWndProc = DefWindowProcA;
    wc.lpszClassName = "d3dx9_test_wc";
    if (!RegisterClass(&wc))
    {
        skip("RegisterClass failed\n");
        return;
    }

    wnd = CreateWindow("d3dx9_test_wc", "d3dx9_test",
                        WS_SYSMENU | WS_POPUP , 0, 0, 640, 480, 0, 0, 0, 0);
    ok(wnd != NULL, "Expected to have a window, received NULL\n");
    if (!wnd)
    {
        skip("Couldn't create application window\n");
        return;
    }

    d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
    {
        skip("Couldn't create IDirect3D9 object\n");
        DestroyWindow(wnd);
        return;
    }

    memset(&d3dpp, 0, sizeof(d3dpp));
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    hr = IDirect3D9_CreateDevice(d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, wnd, D3DCREATE_MIXED_VERTEXPROCESSING, &d3dpp, &device);
    if (FAILED(hr))
    {
        skip("Failed to create IDirect3DDevice9 object %#x\n", hr);
        IDirect3D9_Release(d3d);
        DestroyWindow(wnd);
        return;
    }

    hr = D3DXCreateBuffer(36 * sizeof(DWORD), &ppBuffer);
    ok(hr==D3D_OK, "Expected D3D_OK, received %#x\n", hr);
    if (FAILED(hr)) goto end;

    hr = D3DXCreateBox(device,2.0f,20.0f,4.9f,NULL, &ppBuffer);
    todo_wine ok(hr==D3DERR_INVALIDCALL, "Expected D3DERR_INVALIDCALL, received %#x\n", hr);

    hr = D3DXCreateBox(NULL,22.0f,20.0f,4.9f,&box, &ppBuffer);
    todo_wine ok(hr==D3DERR_INVALIDCALL, "Expected D3DERR_INVALIDCALL, received %#x\n", hr);

    hr = D3DXCreateBox(device,-2.0f,20.0f,4.9f,&box, &ppBuffer);
    todo_wine ok(hr==D3DERR_INVALIDCALL, "Expected D3DERR_INVALIDCALL, received %#x\n", hr);

    hr = D3DXCreateBox(device,22.0f,-20.0f,4.9f,&box, &ppBuffer);
    todo_wine ok(hr==D3DERR_INVALIDCALL, "Expected D3DERR_INVALIDCALL, received %#x\n", hr);

    hr = D3DXCreateBox(device,22.0f,20.0f,-4.9f,&box, &ppBuffer);
    todo_wine ok(hr==D3DERR_INVALIDCALL, "Expected D3DERR_INVALIDCALL, received %#x\n", hr);

    hr = D3DXCreateBox(device,10.9f,20.0f,4.9f,&box, &ppBuffer);
    todo_wine ok(hr==D3D_OK, "Expected D3D_OK, received %#x\n", hr);

    if (FAILED(hr))
    {
        skip("D3DXCreateBox failed\n");
        goto end;
    }

    buffer = ID3DXBuffer_GetBufferPointer(ppBuffer);
    for(i=0; i<36; i++)
        todo_wine ok(adjacency[i]==buffer[i], "expected adjacency %d: %#x, received %#x\n",i,adjacency[i], buffer[i]);

    box->lpVtbl->Release(box);

end:
    IDirect3DDevice9_Release(device);
    IDirect3D9_Release(d3d);
    ID3DXBuffer_Release(ppBuffer);
    DestroyWindow(wnd);
}

struct sincos_table
{
    float *sin;
    float *cos;
};

static void free_sincos_table(struct sincos_table *sincos_table)
{
    HeapFree(GetProcessHeap(), 0, sincos_table->cos);
    HeapFree(GetProcessHeap(), 0, sincos_table->sin);
}

/* pre compute sine and cosine tables; caller must free */
static BOOL compute_sincos_table(struct sincos_table *sincos_table, float angle_start, float angle_step, int n)
{
    float angle;
    int i;

    sincos_table->sin = HeapAlloc(GetProcessHeap(), 0, n * sizeof(*sincos_table->sin));
    if (!sincos_table->sin)
    {
        return FALSE;
    }
    sincos_table->cos = HeapAlloc(GetProcessHeap(), 0, n * sizeof(*sincos_table->cos));
    if (!sincos_table->cos)
    {
        HeapFree(GetProcessHeap(), 0, sincos_table->sin);
        return FALSE;
    }

    angle = angle_start;
    for (i = 0; i < n; i++)
    {
        sincos_table->sin[i] = sin(angle);
        sincos_table->cos[i] = cos(angle);
        angle += angle_step;
    }

    return TRUE;
}

static WORD vertex_index(UINT slices, int slice, int stack)
{
    return stack*slices+slice+1;
}

/* slices = subdivisions along xy plane, stacks = subdivisions along z axis */
static BOOL compute_sphere(struct mesh *mesh, FLOAT radius, UINT slices, UINT stacks)
{
    float theta_step, theta_start;
    struct sincos_table theta;
    float phi_step, phi_start;
    struct sincos_table phi;
    DWORD number_of_vertices, number_of_faces;
    DWORD vertex, face;
    int slice, stack;

    /* theta = angle on xy plane wrt x axis */
    theta_step = M_PI / stacks;
    theta_start = theta_step;

    /* phi = angle on xz plane wrt z axis */
    phi_step = -2 * M_PI / slices;
    phi_start = M_PI / 2;

    if (!compute_sincos_table(&theta, theta_start, theta_step, stacks))
    {
        return FALSE;
    }
    if (!compute_sincos_table(&phi, phi_start, phi_step, slices))
    {
        free_sincos_table(&theta);
        return FALSE;
    }

    number_of_vertices = 2 + slices * (stacks-1);
    number_of_faces = 2 * slices + (stacks - 2) * (2 * slices);

    if (!new_mesh(mesh, number_of_vertices, number_of_faces))
    {
        free_sincos_table(&phi);
        free_sincos_table(&theta);
        return FALSE;
    }

    vertex = 0;
    face = 0;

    mesh->vertices[vertex].normal.x = 0.0f;
    mesh->vertices[vertex].normal.y = 0.0f;
    mesh->vertices[vertex].normal.z = 1.0f;
    mesh->vertices[vertex].position.x = 0.0f;
    mesh->vertices[vertex].position.y = 0.0f;
    mesh->vertices[vertex].position.z = radius;
    vertex++;

    for (stack = 0; stack < stacks - 1; stack++)
    {
        for (slice = 0; slice < slices; slice++)
        {
            mesh->vertices[vertex].normal.x = theta.sin[stack] * phi.cos[slice];
            mesh->vertices[vertex].normal.y = theta.sin[stack] * phi.sin[slice];
            mesh->vertices[vertex].normal.z = theta.cos[stack];
            mesh->vertices[vertex].position.x = radius * theta.sin[stack] * phi.cos[slice];
            mesh->vertices[vertex].position.y = radius * theta.sin[stack] * phi.sin[slice];
            mesh->vertices[vertex].position.z = radius * theta.cos[stack];
            vertex++;

            if (slice > 0)
            {
                if (stack == 0)
                {
                    /* top stack is triangle fan */
                    mesh->faces[face][0] = 0;
                    mesh->faces[face][1] = slice + 1;
                    mesh->faces[face][2] = slice;
                    face++;
                }
                else
                {
                    /* stacks in between top and bottom are quad strips */
                    mesh->faces[face][0] = vertex_index(slices, slice-1, stack-1);
                    mesh->faces[face][1] = vertex_index(slices, slice, stack-1);
                    mesh->faces[face][2] = vertex_index(slices, slice-1, stack);
                    face++;

                    mesh->faces[face][0] = vertex_index(slices, slice, stack-1);
                    mesh->faces[face][1] = vertex_index(slices, slice, stack);
                    mesh->faces[face][2] = vertex_index(slices, slice-1, stack);
                    face++;
                }
            }
        }

        if (stack == 0)
        {
            mesh->faces[face][0] = 0;
            mesh->faces[face][1] = 1;
            mesh->faces[face][2] = slice;
            face++;
        }
        else
        {
            mesh->faces[face][0] = vertex_index(slices, slice-1, stack-1);
            mesh->faces[face][1] = vertex_index(slices, 0, stack-1);
            mesh->faces[face][2] = vertex_index(slices, slice-1, stack);
            face++;

            mesh->faces[face][0] = vertex_index(slices, 0, stack-1);
            mesh->faces[face][1] = vertex_index(slices, 0, stack);
            mesh->faces[face][2] = vertex_index(slices, slice-1, stack);
            face++;
        }
    }

    mesh->vertices[vertex].position.x = 0.0f;
    mesh->vertices[vertex].position.y = 0.0f;
    mesh->vertices[vertex].position.z = -radius;
    mesh->vertices[vertex].normal.x = 0.0f;
    mesh->vertices[vertex].normal.y = 0.0f;
    mesh->vertices[vertex].normal.z = -1.0f;

    /* bottom stack is triangle fan */
    for (slice = 1; slice < slices; slice++)
    {
        mesh->faces[face][0] = vertex_index(slices, slice-1, stack-1);
        mesh->faces[face][1] = vertex_index(slices, slice, stack-1);
        mesh->faces[face][2] = vertex;
        face++;
    }

    mesh->faces[face][0] = vertex_index(slices, slice-1, stack-1);
    mesh->faces[face][1] = vertex_index(slices, 0, stack-1);
    mesh->faces[face][2] = vertex;

    free_sincos_table(&phi);
    free_sincos_table(&theta);

    return TRUE;
}

static void test_sphere(IDirect3DDevice9 *device, FLOAT radius, UINT slices, UINT stacks)
{
    HRESULT hr;
    ID3DXMesh *sphere;
    struct mesh mesh;
    char name[256];

    hr = D3DXCreateSphere(device, radius, slices, stacks, &sphere, NULL);
    ok(hr == D3D_OK, "Got result %x, expected 0 (D3D_OK)\n", hr);
    if (hr != D3D_OK)
    {
        skip("Couldn't create sphere\n");
        return;
    }

    if (!compute_sphere(&mesh, radius, slices, stacks))
    {
        skip("Couldn't create mesh\n");
        sphere->lpVtbl->Release(sphere);
        return;
    }

    mesh.fvf = D3DFVF_XYZ | D3DFVF_NORMAL;

    sprintf(name, "sphere (%g, %u, %u)", radius, slices, stacks);
    compare_mesh(name, sphere, &mesh);

    free_mesh(&mesh);

    sphere->lpVtbl->Release(sphere);
}

static void D3DXCreateSphereTest(void)
{
    HRESULT hr;
    HWND wnd;
    IDirect3D9* d3d;
    IDirect3DDevice9* device;
    D3DPRESENT_PARAMETERS d3dpp;
    ID3DXMesh* sphere = NULL;

    hr = D3DXCreateSphere(NULL, 0.0f, 0, 0, NULL, NULL);
    ok(hr == D3DERR_INVALIDCALL, "Got result %x, expected %x (D3DERR_INVALIDCALL)\n",hr,D3DERR_INVALIDCALL);

    hr = D3DXCreateSphere(NULL, 0.1f, 0, 0, NULL, NULL);
    ok(hr == D3DERR_INVALIDCALL, "Got result %x, expected %x (D3DERR_INVALIDCALL)\n",hr,D3DERR_INVALIDCALL);

    hr = D3DXCreateSphere(NULL, 0.0f, 1, 0, NULL, NULL);
    ok(hr == D3DERR_INVALIDCALL, "Got result %x, expected %x (D3DERR_INVALIDCALL)\n",hr,D3DERR_INVALIDCALL);

    hr = D3DXCreateSphere(NULL, 0.0f, 0, 1, NULL, NULL);
    ok(hr == D3DERR_INVALIDCALL, "Got result %x, expected %x (D3DERR_INVALIDCALL)\n",hr,D3DERR_INVALIDCALL);

    wnd = CreateWindow("static", "d3dx9_test", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL);
    d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!wnd)
    {
        skip("Couldn't create application window\n");
        return;
    }
    if (!d3d)
    {
        skip("Couldn't create IDirect3D9 object\n");
        DestroyWindow(wnd);
        return;
    }

    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    hr = IDirect3D9_CreateDevice(d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, wnd, D3DCREATE_MIXED_VERTEXPROCESSING, &d3dpp, &device);
    if (FAILED(hr))
    {
        skip("Failed to create IDirect3DDevice9 object %#x\n", hr);
        IDirect3D9_Release(d3d);
        DestroyWindow(wnd);
        return;
    }

    hr = D3DXCreateSphere(device, 1.0f, 1, 1, &sphere, NULL);
    ok(hr == D3DERR_INVALIDCALL, "Got result %x, expected %x (D3DERR_INVALIDCALL)\n",hr,D3DERR_INVALIDCALL);

    hr = D3DXCreateSphere(device, 1.0f, 2, 1, &sphere, NULL);
    ok(hr == D3DERR_INVALIDCALL, "Got result %x, expected %x (D3DERR_INVALIDCALL)\n", hr, D3DERR_INVALIDCALL);

    hr = D3DXCreateSphere(device, 1.0f, 1, 2, &sphere, NULL);
    ok(hr == D3DERR_INVALIDCALL, "Got result %x, expected %x (D3DERR_INVALIDCALL)\n", hr, D3DERR_INVALIDCALL);

    hr = D3DXCreateSphere(device, -0.1f, 1, 2, &sphere, NULL);
    ok(hr == D3DERR_INVALIDCALL, "Got result %x, expected %x (D3DERR_INVALIDCALL)\n", hr, D3DERR_INVALIDCALL);

    test_sphere(device, 0.0f, 2, 2);
    test_sphere(device, 1.0f, 2, 2);
    test_sphere(device, 1.0f, 3, 2);
    test_sphere(device, 1.0f, 4, 4);
    test_sphere(device, 1.0f, 3, 4);
    test_sphere(device, 5.0f, 6, 7);
    test_sphere(device, 10.0f, 11, 12);

    IDirect3DDevice9_Release(device);
    IDirect3D9_Release(d3d);
    DestroyWindow(wnd);
}

static BOOL compute_cylinder(struct mesh *mesh, FLOAT radius1, FLOAT radius2, FLOAT length, UINT slices, UINT stacks)
{
    float theta_step, theta_start;
    struct sincos_table theta;
    FLOAT delta_radius, radius, radius_step;
    FLOAT z, z_step, z_normal;
    DWORD number_of_vertices, number_of_faces;
    DWORD vertex, face;
    int slice, stack;

    /* theta = angle on xy plane wrt x axis */
    theta_step = -2 * M_PI / slices;
    theta_start = M_PI / 2;

    if (!compute_sincos_table(&theta, theta_start, theta_step, slices))
    {
        return FALSE;
    }

    number_of_vertices = 2 + (slices * (3 + stacks));
    number_of_faces = 2 * slices + stacks * (2 * slices);

    if (!new_mesh(mesh, number_of_vertices, number_of_faces))
    {
        free_sincos_table(&theta);
        return FALSE;
    }

    vertex = 0;
    face = 0;

    delta_radius = radius1 - radius2;
    radius = radius1;
    radius_step = delta_radius / stacks;

    z = -length / 2;
    z_step = length / stacks;
    z_normal = delta_radius / length;
    if (isnan(z_normal))
    {
        z_normal = 0.0f;
    }

    mesh->vertices[vertex].normal.x = 0.0f;
    mesh->vertices[vertex].normal.y = 0.0f;
    mesh->vertices[vertex].normal.z = -1.0f;
    mesh->vertices[vertex].position.x = 0.0f;
    mesh->vertices[vertex].position.y = 0.0f;
    mesh->vertices[vertex++].position.z = z;

    for (slice = 0; slice < slices; slice++, vertex++)
    {
        mesh->vertices[vertex].normal.x = 0.0f;
        mesh->vertices[vertex].normal.y = 0.0f;
        mesh->vertices[vertex].normal.z = -1.0f;
        mesh->vertices[vertex].position.x = radius * theta.cos[slice];
        mesh->vertices[vertex].position.y = radius * theta.sin[slice];
        mesh->vertices[vertex].position.z = z;

        if (slice > 0)
        {
            mesh->faces[face][0] = 0;
            mesh->faces[face][1] = slice;
            mesh->faces[face++][2] = slice + 1;
        }
    }

    mesh->faces[face][0] = 0;
    mesh->faces[face][1] = slice;
    mesh->faces[face++][2] = 1;

    for (stack = 1; stack <= stacks+1; stack++)
    {
        for (slice = 0; slice < slices; slice++, vertex++)
        {
            mesh->vertices[vertex].normal.x = theta.cos[slice];
            mesh->vertices[vertex].normal.y = theta.sin[slice];
            mesh->vertices[vertex].normal.z = z_normal;
            D3DXVec3Normalize(&mesh->vertices[vertex].normal, &mesh->vertices[vertex].normal);
            mesh->vertices[vertex].position.x = radius * theta.cos[slice];
            mesh->vertices[vertex].position.y = radius * theta.sin[slice];
            mesh->vertices[vertex].position.z = z;

            if (stack > 1 && slice > 0)
            {
                mesh->faces[face][0] = vertex_index(slices, slice-1, stack-1);
                mesh->faces[face][1] = vertex_index(slices, slice-1, stack);
                mesh->faces[face++][2] = vertex_index(slices, slice, stack-1);

                mesh->faces[face][0] = vertex_index(slices, slice, stack-1);
                mesh->faces[face][1] = vertex_index(slices, slice-1, stack);
                mesh->faces[face++][2] = vertex_index(slices, slice, stack);
            }
        }

        if (stack > 1)
        {
            mesh->faces[face][0] = vertex_index(slices, slice-1, stack-1);
            mesh->faces[face][1] = vertex_index(slices, slice-1, stack);
            mesh->faces[face++][2] = vertex_index(slices, 0, stack-1);

            mesh->faces[face][0] = vertex_index(slices, 0, stack-1);
            mesh->faces[face][1] = vertex_index(slices, slice-1, stack);
            mesh->faces[face++][2] = vertex_index(slices, 0, stack);
        }

        if (stack < stacks + 1)
        {
            z += z_step;
            radius -= radius_step;
        }
    }

    for (slice = 0; slice < slices; slice++, vertex++)
    {
        mesh->vertices[vertex].normal.x = 0.0f;
        mesh->vertices[vertex].normal.y = 0.0f;
        mesh->vertices[vertex].normal.z = 1.0f;
        mesh->vertices[vertex].position.x = radius * theta.cos[slice];
        mesh->vertices[vertex].position.y = radius * theta.sin[slice];
        mesh->vertices[vertex].position.z = z;

        if (slice > 0)
        {
            mesh->faces[face][0] = vertex_index(slices, slice-1, stack);
            mesh->faces[face][1] = number_of_vertices - 1;
            mesh->faces[face++][2] = vertex_index(slices, slice, stack);
        }
    }

    mesh->vertices[vertex].position.x = 0.0f;
    mesh->vertices[vertex].position.y = 0.0f;
    mesh->vertices[vertex].position.z = z;
    mesh->vertices[vertex].normal.x = 0.0f;
    mesh->vertices[vertex].normal.y = 0.0f;
    mesh->vertices[vertex].normal.z = 1.0f;

    mesh->faces[face][0] = vertex_index(slices, slice-1, stack);
    mesh->faces[face][1] = number_of_vertices - 1;
    mesh->faces[face][2] = vertex_index(slices, 0, stack);

    free_sincos_table(&theta);

    return TRUE;
}

static void test_cylinder(IDirect3DDevice9 *device, FLOAT radius1, FLOAT radius2, FLOAT length, UINT slices, UINT stacks)
{
    HRESULT hr;
    ID3DXMesh *cylinder;
    struct mesh mesh;
    char name[256];

    hr = D3DXCreateCylinder(device, radius1, radius2, length, slices, stacks, &cylinder, NULL);
    ok(hr == D3D_OK, "Got result %x, expected 0 (D3D_OK)\n", hr);
    if (hr != D3D_OK)
    {
        skip("Couldn't create cylinder\n");
        return;
    }

    if (!compute_cylinder(&mesh, radius1, radius2, length, slices, stacks))
    {
        skip("Couldn't create mesh\n");
        cylinder->lpVtbl->Release(cylinder);
        return;
    }

    mesh.fvf = D3DFVF_XYZ | D3DFVF_NORMAL;

    sprintf(name, "cylinder (%g, %g, %g, %u, %u)", radius1, radius2, length, slices, stacks);
    compare_mesh(name, cylinder, &mesh);

    free_mesh(&mesh);

    cylinder->lpVtbl->Release(cylinder);
}

static void D3DXCreateCylinderTest(void)
{
    HRESULT hr;
    HWND wnd;
    IDirect3D9* d3d;
    IDirect3DDevice9* device;
    D3DPRESENT_PARAMETERS d3dpp;
    ID3DXMesh* cylinder = NULL;

    hr = D3DXCreateCylinder(NULL, 0.0f, 0.0f, 0.0f, 0, 0, NULL, NULL);
    ok(hr == D3DERR_INVALIDCALL, "Got result %x, expected %x (D3DERR_INVALIDCALL)\n",hr,D3DERR_INVALIDCALL);

    hr = D3DXCreateCylinder(NULL, 1.0f, 1.0f, 1.0f, 2, 1, &cylinder, NULL);
    ok(hr == D3DERR_INVALIDCALL, "Got result %x, expected %x (D3DERR_INVALIDCALL)\n",hr,D3DERR_INVALIDCALL);

    wnd = CreateWindow("static", "d3dx9_test", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL);
    d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!wnd)
    {
        skip("Couldn't create application window\n");
        return;
    }
    if (!d3d)
    {
        skip("Couldn't create IDirect3D9 object\n");
        DestroyWindow(wnd);
        return;
    }

    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    hr = IDirect3D9_CreateDevice(d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, wnd, D3DCREATE_MIXED_VERTEXPROCESSING, &d3dpp, &device);
    if (FAILED(hr))
    {
        skip("Failed to create IDirect3DDevice9 object %#x\n", hr);
        IDirect3D9_Release(d3d);
        DestroyWindow(wnd);
        return;
    }

    hr = D3DXCreateCylinder(device, -0.1f, 1.0f, 1.0f, 2, 1, &cylinder, NULL);
    ok(hr == D3DERR_INVALIDCALL, "Got result %x, expected %x (D3DERR_INVALIDCALL)\n",hr,D3DERR_INVALIDCALL);

    hr = D3DXCreateCylinder(device, 0.0f, 1.0f, 1.0f, 2, 1, &cylinder, NULL);
    ok(hr == D3D_OK, "Got result %x, expected 0 (D3D_OK)\n",hr);

    if (SUCCEEDED(hr) && cylinder)
    {
        cylinder->lpVtbl->Release(cylinder);
    }

    hr = D3DXCreateCylinder(device, 1.0f, -0.1f, 1.0f, 2, 1, &cylinder, NULL);
    ok(hr == D3DERR_INVALIDCALL, "Got result %x, expected %x (D3DERR_INVALIDCALL)\n",hr,D3DERR_INVALIDCALL);

    hr = D3DXCreateCylinder(device, 1.0f, 0.0f, 1.0f, 2, 1, &cylinder, NULL);
    ok(hr == D3D_OK, "Got result %x, expected 0 (D3D_OK)\n",hr);

    if (SUCCEEDED(hr) && cylinder)
    {
        cylinder->lpVtbl->Release(cylinder);
    }

    hr = D3DXCreateCylinder(device, 1.0f, 1.0f, -0.1f, 2, 1, &cylinder, NULL);
    ok(hr == D3DERR_INVALIDCALL, "Got result %x, expected %x (D3DERR_INVALIDCALL)\n",hr,D3DERR_INVALIDCALL);

    /* Test with length == 0.0f succeeds */
    hr = D3DXCreateCylinder(device, 1.0f, 1.0f, 0.0f, 2, 1, &cylinder, NULL);
    ok(hr == D3D_OK, "Got result %x, expected 0 (D3D_OK)\n",hr);

    if (SUCCEEDED(hr) && cylinder)
    {
        cylinder->lpVtbl->Release(cylinder);
    }

    hr = D3DXCreateCylinder(device, 1.0f, 1.0f, 1.0f, 1, 1, &cylinder, NULL);
    ok(hr == D3DERR_INVALIDCALL, "Got result %x, expected %x (D3DERR_INVALIDCALL)\n",hr,D3DERR_INVALIDCALL);

    hr = D3DXCreateCylinder(device, 1.0f, 1.0f, 1.0f, 2, 0, &cylinder, NULL);
    ok(hr == D3DERR_INVALIDCALL, "Got result %x, expected %x (D3DERR_INVALIDCALL)\n",hr,D3DERR_INVALIDCALL);

    hr = D3DXCreateCylinder(device, 1.0f, 1.0f, 1.0f, 2, 1, NULL, NULL);
    ok(hr == D3DERR_INVALIDCALL, "Got result %x, expected %x (D3DERR_INVALIDCALL)\n",hr,D3DERR_INVALIDCALL);

    test_cylinder(device, 0.0f, 0.0f, 0.0f, 2, 1);
    test_cylinder(device, 1.0f, 1.0f, 1.0f, 2, 1);
    test_cylinder(device, 1.0f, 1.0f, 2.0f, 3, 4);
    test_cylinder(device, 3.0f, 2.0f, 4.0f, 3, 4);
    test_cylinder(device, 2.0f, 3.0f, 4.0f, 3, 4);
    test_cylinder(device, 3.0f, 4.0f, 5.0f, 11, 20);

    IDirect3DDevice9_Release(device);
    IDirect3D9_Release(d3d);
    DestroyWindow(wnd);
}

struct dynamic_array
{
    int count, capacity;
    void *items;
};

enum pointtype {
    POINTTYPE_CURVE = 0,
    POINTTYPE_CORNER,
    POINTTYPE_CURVE_START,
    POINTTYPE_CURVE_END,
    POINTTYPE_CURVE_MIDDLE,
};

struct point2d
{
    D3DXVECTOR2 pos;
    enum pointtype corner;
};

/* is a dynamic_array */
struct outline
{
    int count, capacity;
    struct point2d *items;
};

/* is a dynamic_array */
struct outline_array
{
    int count, capacity;
    struct outline *items;
};

struct glyphinfo
{
    struct outline_array outlines;
    float offset_x;
};

static BOOL reserve(struct dynamic_array *array, int count, int itemsize)
{
    if (count > array->capacity) {
        void *new_buffer;
        int new_capacity;
        if (array->items && array->capacity) {
            new_capacity = max(array->capacity * 2, count);
            new_buffer = HeapReAlloc(GetProcessHeap(), 0, array->items, new_capacity * itemsize);
        } else {
            new_capacity = max(16, count);
            new_buffer = HeapAlloc(GetProcessHeap(), 0, new_capacity * itemsize);
        }
        if (!new_buffer)
            return FALSE;
        array->items = new_buffer;
        array->capacity = new_capacity;
    }
    return TRUE;
}

static struct point2d *add_point(struct outline *array)
{
    struct point2d *item;

    if (!reserve((struct dynamic_array *)array, array->count + 1, sizeof(array->items[0])))
        return NULL;

    item = &array->items[array->count++];
    ZeroMemory(item, sizeof(*item));
    return item;
}

static struct outline *add_outline(struct outline_array *array)
{
    struct outline *item;

    if (!reserve((struct dynamic_array *)array, array->count + 1, sizeof(array->items[0])))
        return NULL;

    item = &array->items[array->count++];
    ZeroMemory(item, sizeof(*item));
    return item;
}

static inline D3DXVECTOR2 *convert_fixed_to_float(POINTFX *pt, int count, float emsquare)
{
    D3DXVECTOR2 *ret = (D3DXVECTOR2*)pt;
    while (count--) {
        D3DXVECTOR2 *pt_flt = (D3DXVECTOR2*)pt;
        pt_flt->x = (pt->x.value + pt->x.fract / (float)0x10000) / emsquare;
        pt_flt->y = (pt->y.value + pt->y.fract / (float)0x10000) / emsquare;
        pt++;
    }
    return ret;
}

static HRESULT add_bezier_points(struct outline *outline, const D3DXVECTOR2 *p1,
                                 const D3DXVECTOR2 *p2, const D3DXVECTOR2 *p3,
                                 float max_deviation)
{
    D3DXVECTOR2 split1 = {0, 0}, split2 = {0, 0}, middle, vec;
    float deviation;

    D3DXVec2Scale(&split1, D3DXVec2Add(&split1, p1, p2), 0.5f);
    D3DXVec2Scale(&split2, D3DXVec2Add(&split2, p2, p3), 0.5f);
    D3DXVec2Scale(&middle, D3DXVec2Add(&middle, &split1, &split2), 0.5f);

    deviation = D3DXVec2Length(D3DXVec2Subtract(&vec, &middle, p2));
    if (deviation < max_deviation) {
        struct point2d *pt = add_point(outline);
        if (!pt) return E_OUTOFMEMORY;
        pt->pos = *p2;
        pt->corner = POINTTYPE_CURVE;
        /* the end point is omitted because the end line merges into the next segment of
         * the split bezier curve, and the end of the split bezier curve is added outside
         * this recursive function. */
    } else {
        HRESULT hr = add_bezier_points(outline, p1, &split1, &middle, max_deviation);
        if (hr != S_OK) return hr;
        hr = add_bezier_points(outline, &middle, &split2, p3, max_deviation);
        if (hr != S_OK) return hr;
    }

    return S_OK;
}

static inline BOOL is_direction_similar(D3DXVECTOR2 *dir1, D3DXVECTOR2 *dir2, float cos_theta)
{
    /* dot product = cos(theta) */
    return D3DXVec2Dot(dir1, dir2) > cos_theta;
}

static inline D3DXVECTOR2 *unit_vec2(D3DXVECTOR2 *dir, const D3DXVECTOR2 *pt1, const D3DXVECTOR2 *pt2)
{
    return D3DXVec2Normalize(D3DXVec2Subtract(dir, pt2, pt1), dir);
}

static BOOL attempt_line_merge(struct outline *outline,
                               int pt_index,
                               const D3DXVECTOR2 *nextpt,
                               BOOL to_curve)
{
    D3DXVECTOR2 curdir, lastdir;
    struct point2d *prevpt, *pt;
    BOOL ret = FALSE;
    const float cos_half = cos(D3DXToRadian(0.5f));

    pt = &outline->items[pt_index];
    pt_index = (pt_index - 1 + outline->count) % outline->count;
    prevpt = &outline->items[pt_index];

    if (to_curve)
        pt->corner = pt->corner != POINTTYPE_CORNER ? POINTTYPE_CURVE_MIDDLE : POINTTYPE_CURVE_START;

    if (outline->count < 2)
        return FALSE;

    /* remove last point if the next line continues the last line */
    unit_vec2(&lastdir, &prevpt->pos, &pt->pos);
    unit_vec2(&curdir, &pt->pos, nextpt);
    if (is_direction_similar(&lastdir, &curdir, cos_half))
    {
        outline->count--;
        if (pt->corner == POINTTYPE_CURVE_END)
            prevpt->corner = pt->corner;
        if (prevpt->corner == POINTTYPE_CURVE_END && to_curve)
            prevpt->corner = POINTTYPE_CURVE_MIDDLE;
        pt = prevpt;

        ret = TRUE;
        if (outline->count < 2)
            return ret;

        pt_index = (pt_index - 1 + outline->count) % outline->count;
        prevpt = &outline->items[pt_index];
        unit_vec2(&lastdir, &prevpt->pos, &pt->pos);
        unit_vec2(&curdir, &pt->pos, nextpt);
    }
    return ret;
}

static HRESULT create_outline(struct glyphinfo *glyph, void *raw_outline, int datasize,
                              float max_deviation, float emsquare)
{
    const float cos_45 = cos(D3DXToRadian(45.0f));
    const float cos_90 = cos(D3DXToRadian(90.0f));
    TTPOLYGONHEADER *header = (TTPOLYGONHEADER *)raw_outline;

    while ((char *)header < (char *)raw_outline + datasize)
    {
        TTPOLYCURVE *curve = (TTPOLYCURVE *)(header + 1);
        struct point2d *lastpt, *pt;
        D3DXVECTOR2 lastdir;
        D3DXVECTOR2 *pt_flt;
        int j;
        struct outline *outline = add_outline(&glyph->outlines);

        if (!outline)
            return E_OUTOFMEMORY;

        pt = add_point(outline);
        if (!pt)
            return E_OUTOFMEMORY;
        pt_flt = convert_fixed_to_float(&header->pfxStart, 1, emsquare);
        pt->pos = *pt_flt;
        pt->corner = POINTTYPE_CORNER;

        if (header->dwType != TT_POLYGON_TYPE)
            trace("Unknown header type %d\n", header->dwType);

        while ((char *)curve < (char *)header + header->cb)
        {
            D3DXVECTOR2 bezier_start = outline->items[outline->count - 1].pos;
            BOOL to_curve = curve->wType != TT_PRIM_LINE && curve->cpfx > 1;

            if (!curve->cpfx) {
                curve = (TTPOLYCURVE *)&curve->apfx[curve->cpfx];
                continue;
            }

            pt_flt = convert_fixed_to_float(curve->apfx, curve->cpfx, emsquare);

            attempt_line_merge(outline, outline->count - 1, &pt_flt[0], to_curve);

            if (to_curve)
            {
                HRESULT hr;
                int count = curve->cpfx;
                j = 0;

                while (count > 2)
                {
                    D3DXVECTOR2 bezier_end;

                    D3DXVec2Scale(&bezier_end, D3DXVec2Add(&bezier_end, &pt_flt[j], &pt_flt[j+1]), 0.5f);
                    hr = add_bezier_points(outline, &bezier_start, &pt_flt[j], &bezier_end, max_deviation);
                    if (hr != S_OK)
                        return hr;
                    bezier_start = bezier_end;
                    count--;
                    j++;
                }
                hr = add_bezier_points(outline, &bezier_start, &pt_flt[j], &pt_flt[j+1], max_deviation);
                if (hr != S_OK)
                    return hr;

                pt = add_point(outline);
                if (!pt)
                    return E_OUTOFMEMORY;
                j++;
                pt->pos = pt_flt[j];
                pt->corner = POINTTYPE_CURVE_END;
            } else {
                for (j = 0; j < curve->cpfx; j++)
                {
                    pt = add_point(outline);
                    if (!pt)
                        return E_OUTOFMEMORY;
                    pt->pos = pt_flt[j];
                    pt->corner = POINTTYPE_CORNER;
                }
            }

            curve = (TTPOLYCURVE *)&curve->apfx[curve->cpfx];
        }

        /* remove last point if the next line continues the last line */
        if (outline->count >= 3) {
            BOOL to_curve;

            lastpt = &outline->items[outline->count - 1];
            pt = &outline->items[0];
            if (pt->pos.x == lastpt->pos.x && pt->pos.y == lastpt->pos.y) {
                if (lastpt->corner == POINTTYPE_CURVE_END)
                {
                    if (pt->corner == POINTTYPE_CURVE_START)
                        pt->corner = POINTTYPE_CURVE_MIDDLE;
                    else
                        pt->corner = POINTTYPE_CURVE_END;
                }
                outline->count--;
                lastpt = &outline->items[outline->count - 1];
            } else {
                /* outline closed with a line from end to start point */
                attempt_line_merge(outline, outline->count - 1, &pt->pos, FALSE);
            }
            lastpt = &outline->items[0];
            to_curve = lastpt->corner != POINTTYPE_CORNER && lastpt->corner != POINTTYPE_CURVE_END;
            if (lastpt->corner == POINTTYPE_CURVE_START)
                lastpt->corner = POINTTYPE_CORNER;
            pt = &outline->items[1];
            if (attempt_line_merge(outline, 0, &pt->pos, to_curve))
                *lastpt = outline->items[outline->count];
        }

        lastpt = &outline->items[outline->count - 1];
        pt = &outline->items[0];
        unit_vec2(&lastdir, &lastpt->pos, &pt->pos);
        for (j = 0; j < outline->count; j++)
        {
            D3DXVECTOR2 curdir;

            lastpt = pt;
            pt = &outline->items[(j + 1) % outline->count];
            unit_vec2(&curdir, &lastpt->pos, &pt->pos);

            switch (lastpt->corner)
            {
                case POINTTYPE_CURVE_START:
                case POINTTYPE_CURVE_END:
                    if (!is_direction_similar(&lastdir, &curdir, cos_45))
                        lastpt->corner = POINTTYPE_CORNER;
                    break;
                case POINTTYPE_CURVE_MIDDLE:
                    if (!is_direction_similar(&lastdir, &curdir, cos_90))
                        lastpt->corner = POINTTYPE_CORNER;
                    else
                        lastpt->corner = POINTTYPE_CURVE;
                    break;
                default:
                    break;
            }
            lastdir = curdir;
        }

        header = (TTPOLYGONHEADER *)((char *)header + header->cb);
    }
    return S_OK;
}

static BOOL compute_text_mesh(struct mesh *mesh, HDC hdc, LPCSTR text, FLOAT deviation, FLOAT extrusion, FLOAT otmEMSquare)
{
    HRESULT hr = E_FAIL;
    DWORD nb_vertices, nb_faces;
    DWORD nb_corners, nb_outline_points;
    int textlen = 0;
    float offset_x;
    char *raw_outline = NULL;
    struct glyphinfo *glyphs = NULL;
    GLYPHMETRICS gm;
    int i;
    struct vertex *vertex_ptr;
    face *face_ptr;

    if (deviation == 0.0f)
        deviation = 1.0f / otmEMSquare;

    textlen = strlen(text);
    glyphs = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, textlen * sizeof(*glyphs));
    if (!glyphs) {
        hr = E_OUTOFMEMORY;
        goto error;
    }

    offset_x = 0.0f;
    for (i = 0; i < textlen; i++)
    {
        /* get outline points from data returned from GetGlyphOutline */
        const MAT2 identity = {{0, 1}, {0, 0}, {0, 0}, {0, 1}};
        int datasize;

        glyphs[i].offset_x = offset_x;

        datasize = GetGlyphOutline(hdc, text[i], GGO_NATIVE, &gm, 0, NULL, &identity);
        if (datasize < 0) {
            hr = E_FAIL;
            goto error;
        }
        HeapFree(GetProcessHeap(), 0, raw_outline);
        raw_outline = HeapAlloc(GetProcessHeap(), 0, datasize);
        if (!glyphs) {
            hr = E_OUTOFMEMORY;
            goto error;
        }
        datasize = GetGlyphOutline(hdc, text[i], GGO_NATIVE, &gm, datasize, raw_outline, &identity);

        create_outline(&glyphs[i], raw_outline, datasize, deviation, otmEMSquare);

        offset_x += gm.gmCellIncX / (float)otmEMSquare;
    }

    /* corner points need an extra vertex for the different side faces normals */
    nb_corners = 0;
    nb_outline_points = 0;
    for (i = 0; i < textlen; i++)
    {
        int j;
        for (j = 0; j < glyphs[i].outlines.count; j++)
        {
            int k;
            struct outline *outline = &glyphs[i].outlines.items[j];
            nb_outline_points += outline->count;
            nb_corners++; /* first outline point always repeated as a corner */
            for (k = 1; k < outline->count; k++)
                if (outline->items[k].corner)
                    nb_corners++;
        }
    }

    nb_vertices = (nb_outline_points + nb_corners) * 2 + textlen;
    nb_faces = nb_outline_points * 2;

    if (!new_mesh(mesh, nb_vertices, nb_faces))
        goto error;

    /* convert 2D vertices and faces into 3D mesh */
    vertex_ptr = mesh->vertices;
    face_ptr = mesh->faces;
    for (i = 0; i < textlen; i++)
    {
        int j;

        /* side vertices and faces */
        for (j = 0; j < glyphs[i].outlines.count; j++)
        {
            struct vertex *outline_vertices = vertex_ptr;
            struct outline *outline = &glyphs[i].outlines.items[j];
            int k;
            struct point2d *prevpt = &outline->items[outline->count - 1];
            struct point2d *pt = &outline->items[0];

            for (k = 1; k <= outline->count; k++)
            {
                struct vertex vtx;
                struct point2d *nextpt = &outline->items[k % outline->count];
                WORD vtx_idx = vertex_ptr - mesh->vertices;
                D3DXVECTOR2 vec;

                if (pt->corner == POINTTYPE_CURVE_START)
                    D3DXVec2Subtract(&vec, &pt->pos, &prevpt->pos);
                else if (pt->corner)
                    D3DXVec2Subtract(&vec, &nextpt->pos, &pt->pos);
                else
                    D3DXVec2Subtract(&vec, &nextpt->pos, &prevpt->pos);
                D3DXVec2Normalize(&vec, &vec);
                vtx.normal.x = -vec.y;
                vtx.normal.y = vec.x;
                vtx.normal.z = 0;

                vtx.position.x = pt->pos.x + glyphs[i].offset_x;
                vtx.position.y = pt->pos.y;
                vtx.position.z = 0;
                *vertex_ptr++ = vtx;

                vtx.position.z = -extrusion;
                *vertex_ptr++ = vtx;

                vtx.position.x = nextpt->pos.x + glyphs[i].offset_x;
                vtx.position.y = nextpt->pos.y;
                if (pt->corner && nextpt->corner && nextpt->corner != POINTTYPE_CURVE_END) {
                    vtx.position.z = -extrusion;
                    *vertex_ptr++ = vtx;
                    vtx.position.z = 0;
                    *vertex_ptr++ = vtx;

                    (*face_ptr)[0] = vtx_idx;
                    (*face_ptr)[1] = vtx_idx + 2;
                    (*face_ptr)[2] = vtx_idx + 1;
                    face_ptr++;

                    (*face_ptr)[0] = vtx_idx;
                    (*face_ptr)[1] = vtx_idx + 3;
                    (*face_ptr)[2] = vtx_idx + 2;
                    face_ptr++;
                } else {
                    if (nextpt->corner) {
                        if (nextpt->corner == POINTTYPE_CURVE_END) {
                            struct point2d *nextpt2 = &outline->items[(k + 1) % outline->count];
                            D3DXVec2Subtract(&vec, &nextpt2->pos, &nextpt->pos);
                        } else {
                            D3DXVec2Subtract(&vec, &nextpt->pos, &pt->pos);
                        }
                        D3DXVec2Normalize(&vec, &vec);
                        vtx.normal.x = -vec.y;
                        vtx.normal.y = vec.x;

                        vtx.position.z = 0;
                        *vertex_ptr++ = vtx;
                        vtx.position.z = -extrusion;
                        *vertex_ptr++ = vtx;
                    }

                    (*face_ptr)[0] = vtx_idx;
                    (*face_ptr)[1] = vtx_idx + 3;
                    (*face_ptr)[2] = vtx_idx + 1;
                    face_ptr++;

                    (*face_ptr)[0] = vtx_idx;
                    (*face_ptr)[1] = vtx_idx + 2;
                    (*face_ptr)[2] = vtx_idx + 3;
                    face_ptr++;
                }

                prevpt = pt;
                pt = nextpt;
            }
            if (!pt->corner) {
                *vertex_ptr++ = *outline_vertices++;
                *vertex_ptr++ = *outline_vertices++;
            }
        }

        /* FIXME: compute expected faces */
        /* Add placeholder to separate glyph outlines */
        vertex_ptr->position.x = 0;
        vertex_ptr->position.y = 0;
        vertex_ptr->position.z = 0;
        vertex_ptr->normal.x = 0;
        vertex_ptr->normal.y = 0;
        vertex_ptr->normal.z = 1;
        vertex_ptr++;
    }

    hr = D3D_OK;
error:
    if (glyphs) {
        for (i = 0; i < textlen; i++)
        {
            int j;
            for (j = 0; j < glyphs[i].outlines.count; j++)
                HeapFree(GetProcessHeap(), 0, glyphs[i].outlines.items[j].items);
            HeapFree(GetProcessHeap(), 0, glyphs[i].outlines.items);
        }
        HeapFree(GetProcessHeap(), 0, glyphs);
    }
    HeapFree(GetProcessHeap(), 0, raw_outline);

    return hr == D3D_OK;
}

static void compare_text_outline_mesh(const char *name, ID3DXMesh *d3dxmesh, struct mesh *mesh, int textlen, float extrusion)
{
    HRESULT hr;
    DWORD number_of_vertices, number_of_faces;
    IDirect3DVertexBuffer9 *vertex_buffer = NULL;
    IDirect3DIndexBuffer9 *index_buffer = NULL;
    D3DVERTEXBUFFER_DESC vertex_buffer_description;
    D3DINDEXBUFFER_DESC index_buffer_description;
    struct vertex *vertices = NULL;
    face *faces = NULL;
    int expected, i;
    int vtx_idx1, face_idx1, vtx_idx2, face_idx2;

    number_of_vertices = d3dxmesh->lpVtbl->GetNumVertices(d3dxmesh);
    number_of_faces = d3dxmesh->lpVtbl->GetNumFaces(d3dxmesh);

    /* vertex buffer */
    hr = d3dxmesh->lpVtbl->GetVertexBuffer(d3dxmesh, &vertex_buffer);
    ok(hr == D3D_OK, "Test %s, result %x, expected 0 (D3D_OK)\n", name, hr);
    if (hr != D3D_OK)
    {
        skip("Couldn't get vertex buffers\n");
        goto error;
    }

    hr = IDirect3DVertexBuffer9_GetDesc(vertex_buffer, &vertex_buffer_description);
    ok(hr == D3D_OK, "Test %s, result %x, expected 0 (D3D_OK)\n", name, hr);

    if (hr != D3D_OK)
    {
        skip("Couldn't get vertex buffer description\n");
    }
    else
    {
        ok(vertex_buffer_description.Format == D3DFMT_VERTEXDATA, "Test %s, result %x, expected %x (D3DFMT_VERTEXDATA)\n",
           name, vertex_buffer_description.Format, D3DFMT_VERTEXDATA);
        ok(vertex_buffer_description.Type == D3DRTYPE_VERTEXBUFFER, "Test %s, result %x, expected %x (D3DRTYPE_VERTEXBUFFER)\n",
           name, vertex_buffer_description.Type, D3DRTYPE_VERTEXBUFFER);
        ok(vertex_buffer_description.Usage == 0, "Test %s, result %x, expected %x\n", name, vertex_buffer_description.Usage, 0);
        ok(vertex_buffer_description.Pool == D3DPOOL_MANAGED, "Test %s, result %x, expected %x (D3DPOOL_DEFAULT)\n",
           name, vertex_buffer_description.Pool, D3DPOOL_DEFAULT);
        ok(vertex_buffer_description.FVF == mesh->fvf, "Test %s, result %x, expected %x\n",
           name, vertex_buffer_description.FVF, mesh->fvf);
        if (mesh->fvf == 0)
        {
            expected = number_of_vertices * mesh->vertex_size;
        }
        else
        {
            expected = number_of_vertices * D3DXGetFVFVertexSize(mesh->fvf);
        }
        ok(vertex_buffer_description.Size == expected, "Test %s, result %x, expected %x\n",
           name, vertex_buffer_description.Size, expected);
    }

    hr = d3dxmesh->lpVtbl->GetIndexBuffer(d3dxmesh, &index_buffer);
    ok(hr == D3D_OK, "Test %s, result %x, expected 0 (D3D_OK)\n", name, hr);
    if (hr != D3D_OK)
    {
        skip("Couldn't get index buffer\n");
        goto error;
    }

    hr = IDirect3DIndexBuffer9_GetDesc(index_buffer, &index_buffer_description);
    ok(hr == D3D_OK, "Test %s, result %x, expected 0 (D3D_OK)\n", name, hr);

    if (hr != D3D_OK)
    {
        skip("Couldn't get index buffer description\n");
    }
    else
    {
        ok(index_buffer_description.Format == D3DFMT_INDEX16, "Test %s, result %x, expected %x (D3DFMT_INDEX16)\n",
           name, index_buffer_description.Format, D3DFMT_INDEX16);
        ok(index_buffer_description.Type == D3DRTYPE_INDEXBUFFER, "Test %s, result %x, expected %x (D3DRTYPE_INDEXBUFFER)\n",
           name, index_buffer_description.Type, D3DRTYPE_INDEXBUFFER);
        todo_wine ok(index_buffer_description.Usage == 0, "Test %s, result %x, expected %x\n", name, index_buffer_description.Usage, 0);
        ok(index_buffer_description.Pool == D3DPOOL_MANAGED, "Test %s, result %x, expected %x (D3DPOOL_DEFAULT)\n",
           name, index_buffer_description.Pool, D3DPOOL_DEFAULT);
        expected = number_of_faces * sizeof(WORD) * 3;
        ok(index_buffer_description.Size == expected, "Test %s, result %x, expected %x\n",
           name, index_buffer_description.Size, expected);
    }

    /* specify offset and size to avoid potential overruns */
    hr = IDirect3DVertexBuffer9_Lock(vertex_buffer, 0, number_of_vertices * sizeof(D3DXVECTOR3) * 2,
                                     (LPVOID *)&vertices, D3DLOCK_DISCARD);
    ok(hr == D3D_OK, "Test %s, result %x, expected 0 (D3D_OK)\n", name, hr);
    if (hr != D3D_OK)
    {
        skip("Couldn't lock vertex buffer\n");
        goto error;
    }
    hr = IDirect3DIndexBuffer9_Lock(index_buffer, 0, number_of_faces * sizeof(WORD) * 3,
                                    (LPVOID *)&faces, D3DLOCK_DISCARD);
    ok(hr == D3D_OK, "Test %s, result %x, expected 0 (D3D_OK)\n", name, hr);
    if (hr != D3D_OK)
    {
        skip("Couldn't lock index buffer\n");
        goto error;
    }

    face_idx1 = 0;
    vtx_idx2 = 0;
    face_idx2 = 0;
    vtx_idx1 = 0;
    for (i = 0; i < textlen; i++)
    {
        int nb_outline_vertices1, nb_outline_faces1;
        int nb_outline_vertices2, nb_outline_faces2;
        int nb_back_vertices, nb_back_faces;
        int first_vtx1, first_vtx2;
        int first_face1, first_face2;
        int j;

        first_vtx1 = vtx_idx1;
        first_vtx2 = vtx_idx2;
        for (; vtx_idx1 < number_of_vertices; vtx_idx1++) {
            if (vertices[vtx_idx1].normal.z != 0)
                break;
        }
        for (; vtx_idx2 < mesh->number_of_vertices; vtx_idx2++) {
            if (mesh->vertices[vtx_idx2].normal.z != 0)
                break;
        }
        nb_outline_vertices1 = vtx_idx1 - first_vtx1;
        nb_outline_vertices2 = vtx_idx2 - first_vtx2;
        ok(nb_outline_vertices1 == nb_outline_vertices2,
           "Test %s, glyph %d, outline vertex count result %d, expected %d\n", name, i,
           nb_outline_vertices1, nb_outline_vertices2);

        for (j = 0; j < min(nb_outline_vertices1, nb_outline_vertices2); j++)
        {
            vtx_idx1 = first_vtx1 + j;
            vtx_idx2 = first_vtx2 + j;
            ok(compare_vec3(vertices[vtx_idx1].position, mesh->vertices[vtx_idx2].position),
               "Test %s, glyph %d, vertex position %d, result (%g, %g, %g), expected (%g, %g, %g)\n", name, i, vtx_idx1,
               vertices[vtx_idx1].position.x, vertices[vtx_idx1].position.y, vertices[vtx_idx1].position.z,
               mesh->vertices[vtx_idx2].position.x, mesh->vertices[vtx_idx2].position.y, mesh->vertices[vtx_idx2].position.z);
            ok(compare_vec3(vertices[vtx_idx1].normal, mesh->vertices[first_vtx2 + j].normal),
               "Test %s, glyph %d, vertex normal %d, result (%g, %g, %g), expected (%g, %g, %g)\n", name, i, vtx_idx1,
               vertices[vtx_idx1].normal.x, vertices[vtx_idx1].normal.y, vertices[vtx_idx1].normal.z,
               mesh->vertices[vtx_idx2].normal.x, mesh->vertices[vtx_idx2].normal.y, mesh->vertices[vtx_idx2].normal.z);
        }
        vtx_idx1 = first_vtx1 + nb_outline_vertices1;
        vtx_idx2 = first_vtx2 + nb_outline_vertices2;

        first_face1 = face_idx1;
        first_face2 = face_idx2;
        for (; face_idx1 < number_of_faces; face_idx1++)
        {
            if (faces[face_idx1][0] >= vtx_idx1 ||
                faces[face_idx1][1] >= vtx_idx1 ||
                faces[face_idx1][2] >= vtx_idx1)
                break;
        }
        for (; face_idx2 < mesh->number_of_faces; face_idx2++)
        {
            if (mesh->faces[face_idx2][0] >= vtx_idx2 ||
                mesh->faces[face_idx2][1] >= vtx_idx2 ||
                mesh->faces[face_idx2][2] >= vtx_idx2)
                break;
        }
        nb_outline_faces1 = face_idx1 - first_face1;
        nb_outline_faces2 = face_idx2 - first_face2;
        ok(nb_outline_faces1 == nb_outline_faces2,
           "Test %s, glyph %d, outline face count result %d, expected %d\n", name, i,
           nb_outline_faces1, nb_outline_faces2);

        for (j = 0; j < min(nb_outline_faces1, nb_outline_faces2); j++)
        {
            face_idx1 = first_face1 + j;
            face_idx2 = first_face2 + j;
            ok(faces[face_idx1][0] - first_vtx1 == mesh->faces[face_idx2][0] - first_vtx2 &&
               faces[face_idx1][1] - first_vtx1 == mesh->faces[face_idx2][1] - first_vtx2 &&
               faces[face_idx1][2] - first_vtx1 == mesh->faces[face_idx2][2] - first_vtx2,
               "Test %s, glyph %d, face %d, result (%d, %d, %d), expected (%d, %d, %d)\n", name, i, face_idx1,
               faces[face_idx1][0], faces[face_idx1][1], faces[face_idx1][2],
               mesh->faces[face_idx2][0] - first_vtx2 + first_vtx1,
               mesh->faces[face_idx2][1] - first_vtx2 + first_vtx1,
               mesh->faces[face_idx2][2] - first_vtx2 + first_vtx1);
        }
        face_idx1 = first_face1 + nb_outline_faces1;
        face_idx2 = first_face2 + nb_outline_faces2;

        /* partial test on back vertices and faces  */
        first_vtx1 = vtx_idx1;
        for (; vtx_idx1 < number_of_vertices; vtx_idx1++) {
            struct vertex vtx;

            if (vertices[vtx_idx1].normal.z != 1.0f)
                break;

            vtx.position.z = 0.0f;
            vtx.normal.x = 0.0f;
            vtx.normal.y = 0.0f;
            vtx.normal.z = 1.0f;
            ok(compare(vertices[vtx_idx1].position.z, vtx.position.z),
               "Test %s, glyph %d, vertex position.z %d, result %g, expected %g\n", name, i, vtx_idx1,
               vertices[vtx_idx1].position.z, vtx.position.z);
            ok(compare_vec3(vertices[vtx_idx1].normal, vtx.normal),
               "Test %s, glyph %d, vertex normal %d, result (%g, %g, %g), expected (%g, %g, %g)\n", name, i, vtx_idx1,
               vertices[vtx_idx1].normal.x, vertices[vtx_idx1].normal.y, vertices[vtx_idx1].normal.z,
               vtx.normal.x, vtx.normal.y, vtx.normal.z);
        }
        nb_back_vertices = vtx_idx1 - first_vtx1;
        first_face1 = face_idx1;
        for (; face_idx1 < number_of_faces; face_idx1++)
        {
            const D3DXVECTOR3 *vtx1, *vtx2, *vtx3;
            D3DXVECTOR3 normal;
            D3DXVECTOR3 v1 = {0, 0, 0};
            D3DXVECTOR3 v2 = {0, 0, 0};
            D3DXVECTOR3 forward = {0.0f, 0.0f, 1.0f};

            if (faces[face_idx1][0] >= vtx_idx1 ||
                faces[face_idx1][1] >= vtx_idx1 ||
                faces[face_idx1][2] >= vtx_idx1)
                break;

            vtx1 = &vertices[faces[face_idx1][0]].position;
            vtx2 = &vertices[faces[face_idx1][1]].position;
            vtx3 = &vertices[faces[face_idx1][2]].position;

            D3DXVec3Subtract(&v1, vtx2, vtx1);
            D3DXVec3Subtract(&v2, vtx3, vtx2);
            D3DXVec3Cross(&normal, &v1, &v2);
            D3DXVec3Normalize(&normal, &normal);
            ok(compare_vec3(normal, forward),
               "Test %s, glyph %d, face %d normal, result (%g, %g, %g), expected (%g, %g, %g)\n", name, i, face_idx1,
               normal.x, normal.y, normal.z, forward.x, forward.y, forward.z);
        }
        nb_back_faces = face_idx1 - first_face1;

        /* compare front and back faces & vertices */
        if (extrusion == 0.0f) {
            /* Oddly there are only back faces in this case */
            nb_back_vertices /= 2;
            nb_back_faces /= 2;
            face_idx1 -= nb_back_faces;
            vtx_idx1 -= nb_back_vertices;
        }
        for (j = 0; j < nb_back_vertices; j++)
        {
            struct vertex vtx = vertices[first_vtx1];
            vtx.position.z = -extrusion;
            vtx.normal.x = 0.0f;
            vtx.normal.y = 0.0f;
            vtx.normal.z = extrusion == 0.0f ? 1.0f : -1.0f;
            ok(compare_vec3(vertices[vtx_idx1].position, vtx.position),
               "Test %s, glyph %d, vertex position %d, result (%g, %g, %g), expected (%g, %g, %g)\n", name, i, vtx_idx1,
               vertices[vtx_idx1].position.x, vertices[vtx_idx1].position.y, vertices[vtx_idx1].position.z,
               vtx.position.x, vtx.position.y, vtx.position.z);
            ok(compare_vec3(vertices[vtx_idx1].normal, vtx.normal),
               "Test %s, glyph %d, vertex normal %d, result (%g, %g, %g), expected (%g, %g, %g)\n", name, i, vtx_idx1,
               vertices[vtx_idx1].normal.x, vertices[vtx_idx1].normal.y, vertices[vtx_idx1].normal.z,
               vtx.normal.x, vtx.normal.y, vtx.normal.z);
            vtx_idx1++;
            first_vtx1++;
        }
        for (j = 0; j < nb_back_faces; j++)
        {
            int f1, f2;
            if (extrusion == 0.0f) {
                f1 = 1;
                f2 = 2;
            } else {
                f1 = 2;
                f2 = 1;
            }
            ok(faces[face_idx1][0] == faces[first_face1][0] + nb_back_vertices &&
               faces[face_idx1][1] == faces[first_face1][f1] + nb_back_vertices &&
               faces[face_idx1][2] == faces[first_face1][f2] + nb_back_vertices,
               "Test %s, glyph %d, face %d, result (%d, %d, %d), expected (%d, %d, %d)\n", name, i, face_idx1,
               faces[face_idx1][0], faces[face_idx1][1], faces[face_idx1][2],
               faces[first_face1][0] - nb_back_faces,
               faces[first_face1][f1] - nb_back_faces,
               faces[first_face1][f2] - nb_back_faces);
            first_face1++;
            face_idx1++;
        }

        /* skip to the outline for the next glyph */
        for (; vtx_idx2 < mesh->number_of_vertices; vtx_idx2++) {
            if (mesh->vertices[vtx_idx2].normal.z == 0)
                break;
        }
        for (; face_idx2 < mesh->number_of_faces; face_idx2++)
        {
            if (mesh->faces[face_idx2][0] >= vtx_idx2 ||
                mesh->faces[face_idx2][1] >= vtx_idx2 ||
                mesh->faces[face_idx2][2] >= vtx_idx2) break;
        }
    }

error:
    if (vertices) IDirect3DVertexBuffer9_Unlock(vertex_buffer);
    if (faces) IDirect3DIndexBuffer9_Unlock(index_buffer);
    if (index_buffer) IDirect3DIndexBuffer9_Release(index_buffer);
    if (vertex_buffer) IDirect3DVertexBuffer9_Release(vertex_buffer);
}

static void test_createtext(IDirect3DDevice9 *device, HDC hdc, LPCSTR text, FLOAT deviation, FLOAT extrusion)
{
    HRESULT hr;
    ID3DXMesh *d3dxmesh;
    struct mesh mesh;
    char name[256];
    OUTLINETEXTMETRIC otm;
    GLYPHMETRICS gm;
    GLYPHMETRICSFLOAT *glyphmetrics_float = HeapAlloc(GetProcessHeap(), 0, sizeof(GLYPHMETRICSFLOAT) * strlen(text));
    int i;
    LOGFONT lf;
    HFONT font = NULL, oldfont = NULL;

    sprintf(name, "text ('%s', %f, %f)", text, deviation, extrusion);

    hr = D3DXCreateText(device, hdc, text, deviation, extrusion, &d3dxmesh, NULL, glyphmetrics_float);
    ok(hr == D3D_OK, "Got result %x, expected 0 (D3D_OK)\n", hr);
    if (hr != D3D_OK)
    {
        skip("Couldn't create text with D3DXCreateText\n");
        return;
    }

    /* must select a modified font having lfHeight = otm.otmEMSquare before
     * calling GetGlyphOutline to get the expected values */
    if (!GetObject(GetCurrentObject(hdc, OBJ_FONT), sizeof(lf), &lf) ||
        !GetOutlineTextMetrics(hdc, sizeof(otm), &otm))
    {
        d3dxmesh->lpVtbl->Release(d3dxmesh);
        skip("Couldn't get text outline\n");
        return;
    }
    lf.lfHeight = otm.otmEMSquare;
    lf.lfWidth = 0;
    font = CreateFontIndirect(&lf);
    if (!font) {
        d3dxmesh->lpVtbl->Release(d3dxmesh);
        skip("Couldn't create the modified font\n");
        return;
    }
    oldfont = SelectObject(hdc, font);

    for (i = 0; i < strlen(text); i++)
    {
        const MAT2 identity = {{0, 1}, {0, 0}, {0, 0}, {0, 1}};
        GetGlyphOutlineA(hdc, text[i], GGO_NATIVE, &gm, 0, NULL, &identity);
        compare_float(glyphmetrics_float[i].gmfBlackBoxX, gm.gmBlackBoxX / (float)otm.otmEMSquare);
        compare_float(glyphmetrics_float[i].gmfBlackBoxY, gm.gmBlackBoxY / (float)otm.otmEMSquare);
        compare_float(glyphmetrics_float[i].gmfptGlyphOrigin.x, gm.gmptGlyphOrigin.x / (float)otm.otmEMSquare);
        compare_float(glyphmetrics_float[i].gmfptGlyphOrigin.y, gm.gmptGlyphOrigin.y / (float)otm.otmEMSquare);
        compare_float(glyphmetrics_float[i].gmfCellIncX, gm.gmCellIncX / (float)otm.otmEMSquare);
        compare_float(glyphmetrics_float[i].gmfCellIncY, gm.gmCellIncY / (float)otm.otmEMSquare);
    }

    ZeroMemory(&mesh, sizeof(mesh));
    if (!compute_text_mesh(&mesh, hdc, text, deviation, extrusion, otm.otmEMSquare))
    {
        skip("Couldn't create mesh\n");
        d3dxmesh->lpVtbl->Release(d3dxmesh);
        return;
    }
    mesh.fvf = D3DFVF_XYZ | D3DFVF_NORMAL;

    compare_text_outline_mesh(name, d3dxmesh, &mesh, strlen(text), extrusion);

    free_mesh(&mesh);

    d3dxmesh->lpVtbl->Release(d3dxmesh);
    SelectObject(hdc, oldfont);
    HeapFree(GetProcessHeap(), 0, glyphmetrics_float);
}

static void D3DXCreateTextTest(void)
{
    HRESULT hr;
    HWND wnd;
    HDC hdc;
    IDirect3D9* d3d;
    IDirect3DDevice9* device;
    D3DPRESENT_PARAMETERS d3dpp;
    ID3DXMesh* d3dxmesh = NULL;
    HFONT hFont;
    OUTLINETEXTMETRIC otm;
    int number_of_vertices;
    int number_of_faces;

    wnd = CreateWindow("static", "d3dx9_test", WS_POPUP, 0, 0, 1000, 1000, NULL, NULL, NULL, NULL);
    d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!wnd)
    {
        skip("Couldn't create application window\n");
        return;
    }
    if (!d3d)
    {
        skip("Couldn't create IDirect3D9 object\n");
        DestroyWindow(wnd);
        return;
    }

    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    hr = IDirect3D9_CreateDevice(d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, wnd, D3DCREATE_MIXED_VERTEXPROCESSING, &d3dpp, &device);
    if (FAILED(hr))
    {
        skip("Failed to create IDirect3DDevice9 object %#x\n", hr);
        IDirect3D9_Release(d3d);
        DestroyWindow(wnd);
        return;
    }

    hdc = CreateCompatibleDC(NULL);

    hFont = CreateFont(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                       "Arial");
    SelectObject(hdc, hFont);
    GetOutlineTextMetrics(hdc, sizeof(otm), &otm);

    hr = D3DXCreateText(device, hdc, "wine", 0.001f, 0.4f, NULL, NULL, NULL);
    ok(hr == D3DERR_INVALIDCALL, "Got result %x, expected %x (D3DERR_INVALIDCALL)\n", hr, D3DERR_INVALIDCALL);

    /* D3DXCreateTextA page faults from passing NULL text */

    hr = D3DXCreateTextW(device, hdc, NULL, 0.001f, 0.4f, &d3dxmesh, NULL, NULL);
    ok(hr == D3DERR_INVALIDCALL, "Got result %x, expected %x (D3DERR_INVALIDCALL)\n", hr, D3DERR_INVALIDCALL);

    hr = D3DXCreateText(device, hdc, "", 0.001f, 0.4f, &d3dxmesh, NULL, NULL);
    ok(hr == D3DERR_INVALIDCALL, "Got result %x, expected %x (D3DERR_INVALIDCALL)\n", hr, D3DERR_INVALIDCALL);

    hr = D3DXCreateText(device, hdc, " ", 0.001f, 0.4f, &d3dxmesh, NULL, NULL);
    ok(hr == D3DERR_INVALIDCALL, "Got result %x, expected %x (D3DERR_INVALIDCALL)\n", hr, D3DERR_INVALIDCALL);

    hr = D3DXCreateText(NULL, hdc, "wine", 0.001f, 0.4f, &d3dxmesh, NULL, NULL);
    ok(hr == D3DERR_INVALIDCALL, "Got result %x, expected %x (D3DERR_INVALIDCALL)\n", hr, D3DERR_INVALIDCALL);

    hr = D3DXCreateText(device, NULL, "wine", 0.001f, 0.4f, &d3dxmesh, NULL, NULL);
    ok(hr == D3DERR_INVALIDCALL, "Got result %x, expected %x (D3DERR_INVALIDCALL)\n", hr, D3DERR_INVALIDCALL);

    hr = D3DXCreateText(device, hdc, "wine", -FLT_MIN, 0.4f, &d3dxmesh, NULL, NULL);
    ok(hr == D3DERR_INVALIDCALL, "Got result %x, expected %x (D3DERR_INVALIDCALL)\n", hr, D3DERR_INVALIDCALL);

    hr = D3DXCreateText(device, hdc, "wine", 0.001f, -FLT_MIN, &d3dxmesh, NULL, NULL);
    ok(hr == D3DERR_INVALIDCALL, "Got result %x, expected %x (D3DERR_INVALIDCALL)\n", hr, D3DERR_INVALIDCALL);

    /* deviation = 0.0f treated as if deviation = 1.0f / otm.otmEMSquare */
    hr = D3DXCreateText(device, hdc, "wine", 1.0f / otm.otmEMSquare, 0.4f, &d3dxmesh, NULL, NULL);
    ok(hr == D3D_OK, "Got result %x, expected %x (D3D_OK)\n", hr, D3D_OK);
    number_of_vertices = d3dxmesh->lpVtbl->GetNumVertices(d3dxmesh);
    number_of_faces = d3dxmesh->lpVtbl->GetNumFaces(d3dxmesh);
    if (SUCCEEDED(hr) && d3dxmesh) d3dxmesh->lpVtbl->Release(d3dxmesh);

    hr = D3DXCreateText(device, hdc, "wine", 0.0f, 0.4f, &d3dxmesh, NULL, NULL);
    ok(hr == D3D_OK, "Got result %x, expected %x (D3D_OK)\n", hr, D3D_OK);
    ok(number_of_vertices == d3dxmesh->lpVtbl->GetNumVertices(d3dxmesh),
       "Got %d vertices, expected %d\n",
       d3dxmesh->lpVtbl->GetNumVertices(d3dxmesh), number_of_vertices);
    ok(number_of_faces == d3dxmesh->lpVtbl->GetNumFaces(d3dxmesh),
       "Got %d faces, expected %d\n",
       d3dxmesh->lpVtbl->GetNumVertices(d3dxmesh), number_of_faces);
    if (SUCCEEDED(hr) && d3dxmesh) d3dxmesh->lpVtbl->Release(d3dxmesh);

#if 0
    /* too much detail requested, so will appear to hang */
    trace("Waiting for D3DXCreateText to finish with deviation = FLT_MIN ...\n");
    hr = D3DXCreateText(device, hdc, "wine", FLT_MIN, 0.4f, &d3dxmesh, NULL, NULL);
    ok(hr == D3D_OK, "Got result %x, expected %x (D3D_OK)\n", hr, D3D_OK);
    if (SUCCEEDED(hr) && d3dxmesh) d3dxmesh->lpVtbl->Release(d3dxmesh);
    trace("D3DXCreateText finish with deviation = FLT_MIN\n");
#endif

    hr = D3DXCreateText(device, hdc, "wine", 0.001f, 0.4f, &d3dxmesh, NULL, NULL);
    ok(hr == D3D_OK, "Got result %x, expected %x (D3D_OK)\n", hr, D3D_OK);
    if (SUCCEEDED(hr) && d3dxmesh) d3dxmesh->lpVtbl->Release(d3dxmesh);

    test_createtext(device, hdc, "wine", FLT_MAX, 0.4f);
    test_createtext(device, hdc, "wine", 0.001f, FLT_MIN);
    test_createtext(device, hdc, "wine", 0.001f, 0.0f);
    test_createtext(device, hdc, "wine", 0.001f, FLT_MAX);
    test_createtext(device, hdc, "wine", 0.0f, 1.0f);

    DeleteDC(hdc);

    IDirect3DDevice9_Release(device);
    IDirect3D9_Release(d3d);
    DestroyWindow(wnd);
}

static void test_get_decl_length(void)
{
    static const D3DVERTEXELEMENT9 declaration1[] =
    {
        {0, 0, D3DDECLTYPE_FLOAT1, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {1, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {2, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {3, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {4, 0, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {5, 0, D3DDECLTYPE_UBYTE4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {6, 0, D3DDECLTYPE_SHORT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {7, 0, D3DDECLTYPE_SHORT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {8, 0, D3DDECLTYPE_UBYTE4N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {9, 0, D3DDECLTYPE_SHORT2N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {10, 0, D3DDECLTYPE_SHORT4N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {11, 0, D3DDECLTYPE_UDEC3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {12, 0, D3DDECLTYPE_DEC3N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {13, 0, D3DDECLTYPE_FLOAT16_2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {14, 0, D3DDECLTYPE_FLOAT16_4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        D3DDECL_END(),
    };
    static const D3DVERTEXELEMENT9 declaration2[] =
    {
        {0, 8, D3DDECLTYPE_FLOAT1, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {1, 8, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {2, 8, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {3, 8, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {4, 8, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {5, 8, D3DDECLTYPE_UBYTE4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {6, 8, D3DDECLTYPE_SHORT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {7, 8, D3DDECLTYPE_SHORT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {0, 8, D3DDECLTYPE_UBYTE4N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {1, 8, D3DDECLTYPE_SHORT2N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {2, 8, D3DDECLTYPE_SHORT4N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {3, 8, D3DDECLTYPE_UDEC3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {4, 8, D3DDECLTYPE_DEC3N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {5, 8, D3DDECLTYPE_FLOAT16_2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {6, 8, D3DDECLTYPE_FLOAT16_4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {7, 8, D3DDECLTYPE_FLOAT1, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        D3DDECL_END(),
    };
    UINT size;

    size = D3DXGetDeclLength(declaration1);
    ok(size == 15, "Got size %u, expected 15.\n", size);

    size = D3DXGetDeclLength(declaration2);
    ok(size == 16, "Got size %u, expected 16.\n", size);
}

static void test_get_decl_vertex_size(void)
{
    static const D3DVERTEXELEMENT9 declaration1[] =
    {
        {0, 0, D3DDECLTYPE_FLOAT1, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {1, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {2, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {3, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {4, 0, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {5, 0, D3DDECLTYPE_UBYTE4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {6, 0, D3DDECLTYPE_SHORT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {7, 0, D3DDECLTYPE_SHORT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {8, 0, D3DDECLTYPE_UBYTE4N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {9, 0, D3DDECLTYPE_SHORT2N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {10, 0, D3DDECLTYPE_SHORT4N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {11, 0, D3DDECLTYPE_UDEC3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {12, 0, D3DDECLTYPE_DEC3N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {13, 0, D3DDECLTYPE_FLOAT16_2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {14, 0, D3DDECLTYPE_FLOAT16_4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        D3DDECL_END(),
    };
    static const D3DVERTEXELEMENT9 declaration2[] =
    {
        {0, 8, D3DDECLTYPE_FLOAT1, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {1, 8, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {2, 8, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {3, 8, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {4, 8, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {5, 8, D3DDECLTYPE_UBYTE4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {6, 8, D3DDECLTYPE_SHORT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {7, 8, D3DDECLTYPE_SHORT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {0, 8, D3DDECLTYPE_UBYTE4N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {1, 8, D3DDECLTYPE_SHORT2N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {2, 8, D3DDECLTYPE_SHORT4N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {3, 8, D3DDECLTYPE_UDEC3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {4, 8, D3DDECLTYPE_DEC3N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {5, 8, D3DDECLTYPE_FLOAT16_2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {6, 8, D3DDECLTYPE_FLOAT16_4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {7, 8, D3DDECLTYPE_FLOAT1, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        D3DDECL_END(),
    };
    static const UINT sizes1[] =
    {
        4,  8,  12, 16,
        4,  4,  4,  8,
        4,  4,  8,  4,
        4,  4,  8,  0,
    };
    static const UINT sizes2[] =
    {
        12, 16, 20, 24,
        12, 12, 16, 16,
    };
    unsigned int i;
    UINT size;

    size = D3DXGetDeclVertexSize(NULL, 0);
    ok(size == 0, "Got size %#x, expected 0.\n", size);

    for (i = 0; i < 16; ++i)
    {
        size = D3DXGetDeclVertexSize(declaration1, i);
        ok(size == sizes1[i], "Got size %u for stream %u, expected %u.\n", size, i, sizes1[i]);
    }

    for (i = 0; i < 8; ++i)
    {
        size = D3DXGetDeclVertexSize(declaration2, i);
        ok(size == sizes2[i], "Got size %u for stream %u, expected %u.\n", size, i, sizes2[i]);
    }
}

static void D3DXGenerateAdjacencyTest(void)
{
    HRESULT hr;
    HWND wnd;
    IDirect3D9 *d3d;
    IDirect3DDevice9 *device;
    D3DPRESENT_PARAMETERS d3dpp;
    ID3DXMesh *d3dxmesh = NULL;
    D3DXVECTOR3 *vertices = NULL;
    WORD *indices = NULL;
    int i;
    struct {
        DWORD num_vertices;
        D3DXVECTOR3 vertices[6];
        DWORD num_faces;
        WORD indices[3 * 3];
        FLOAT epsilon;
        DWORD adjacency[3 * 3];
    } test_data[] = {
        { /* for epsilon < 0, indices must match for faces to be adjacent */
            4, {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {1.0, 1.0, 0.0}, {0.0, 1.0, 0.0}},
            2, {0, 1, 2,  0, 2, 3},
            -1.0,
            {-1, -1, 1,  0, -1, -1},
        },
        {
            6, {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {1.0, 1.0, 0.0}, {0.0, 0.0, 0.0}, {1.0, 1.0, 0.0}, {0.0, 1.0, 0.0}},
            2, {0, 1, 2,  3, 4, 5},
            -1.0,
            {-1, -1, -1,  -1, -1, -1},
        },
        { /* for epsilon == 0, indices or vertices must match for faces to be adjacent */
            6, {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {1.0, 1.0, 0.0}, {0.0, 0.0, 0.0}, {1.0, 1.0, 0.0}, {0.0, 1.0, 0.0}},
            2, {0, 1, 2,  3, 4, 5},
            0.0,
            {-1, -1, 1,  0, -1, -1},
        },
        { /* for epsilon > 0, vertices must be less than (but NOT equal to) epsilon distance away */
            6, {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {1.0, 1.0, 0.0}, {0.0, 0.0, 0.25}, {1.0, 1.0, 0.25}, {0.0, 1.0, 0.25}},
            2, {0, 1, 2,  3, 4, 5},
            0.25,
            {-1, -1, -1,  -1, -1, -1},
        },
        { /* for epsilon > 0, vertices must be less than (but NOT equal to) epsilon distance away */
            6, {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {1.0, 1.0, 0.0}, {0.0, 0.0, 0.25}, {1.0, 1.0, 0.25}, {0.0, 1.0, 0.25}},
            2, {0, 1, 2,  3, 4, 5},
            0.250001,
            {-1, -1, 1,  0, -1, -1},
        },
        { /* length between vertices are compared to epsilon, not the individual dimension deltas */
            6, {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {1.0, 1.0, 0.0}, {0.0, 0.25, 0.25}, {1.0, 1.25, 0.25}, {0.0, 1.25, 0.25}},
            2, {0, 1, 2,  3, 4, 5},
            0.353, /* < sqrt(0.25*0.25 + 0.25*0.25) */
            {-1, -1, -1,  -1, -1, -1},
        },
        {
            6, {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {1.0, 1.0, 0.0}, {0.0, 0.25, 0.25}, {1.0, 1.25, 0.25}, {0.0, 1.25, 0.25}},
            2, {0, 1, 2,  3, 4, 5},
            0.354, /* > sqrt(0.25*0.25 + 0.25*0.25) */
            {-1, -1, 1,  0, -1, -1},
        },
        { /* adjacent faces must have opposite winding orders at the shared edge */
            4, {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {1.0, 1.0, 0.0}, {0.0, 1.0, 0.0}},
            2, {0, 1, 2,  0, 3, 2},
            0.0,
            {-1, -1, -1,  -1, -1, -1},
        },
    };

    wnd = CreateWindow("static", "d3dx9_test", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL);
    if (!wnd)
    {
        skip("Couldn't create application window\n");
        return;
    }
    d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
    {
        skip("Couldn't create IDirect3D9 object\n");
        DestroyWindow(wnd);
        return;
    }

    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    hr = IDirect3D9_CreateDevice(d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, wnd, D3DCREATE_MIXED_VERTEXPROCESSING, &d3dpp, &device);
    if (FAILED(hr))
    {
        skip("Failed to create IDirect3DDevice9 object %#x\n", hr);
        IDirect3D9_Release(d3d);
        DestroyWindow(wnd);
        return;
    }

    for (i = 0; i < ARRAY_SIZE(test_data); i++)
    {
        DWORD adjacency[ARRAY_SIZE(test_data[0].adjacency)];
        int j;

        if (d3dxmesh) d3dxmesh->lpVtbl->Release(d3dxmesh);
        d3dxmesh = NULL;

        hr = D3DXCreateMeshFVF(test_data[i].num_faces, test_data[i].num_vertices, 0, D3DFVF_XYZ, device, &d3dxmesh);
        ok(hr == D3D_OK, "Got result %x, expected %x (D3D_OK)\n", hr, D3D_OK);

        hr = d3dxmesh->lpVtbl->LockVertexBuffer(d3dxmesh, D3DLOCK_DISCARD, (void**)&vertices);
        ok(hr == D3D_OK, "test %d: Got result %x, expected %x (D3D_OK)\n", i, hr, D3D_OK);
        if (FAILED(hr)) continue;
        CopyMemory(vertices, test_data[i].vertices, test_data[i].num_vertices * sizeof(test_data[0].vertices[0]));
        d3dxmesh->lpVtbl->UnlockVertexBuffer(d3dxmesh);

        hr = d3dxmesh->lpVtbl->LockIndexBuffer(d3dxmesh, D3DLOCK_DISCARD, (void**)&indices);
        ok(hr == D3D_OK, "test %d: Got result %x, expected %x (D3D_OK)\n", i, hr, D3D_OK);
        if (FAILED(hr)) continue;
        CopyMemory(indices, test_data[i].indices, test_data[i].num_faces * 3 * sizeof(test_data[0].indices[0]));
        d3dxmesh->lpVtbl->UnlockIndexBuffer(d3dxmesh);

        if (i == 0) {
            hr = d3dxmesh->lpVtbl->GenerateAdjacency(d3dxmesh, 0.0f, NULL);
            ok(hr == D3DERR_INVALIDCALL, "Got result %x, expected %x (D3DERR_INVALIDCALL)\n", hr, D3DERR_INVALIDCALL);
        }

        hr = d3dxmesh->lpVtbl->GenerateAdjacency(d3dxmesh, test_data[i].epsilon, adjacency);
        ok(hr == D3D_OK, "Got result %x, expected %x (D3D_OK)\n", hr, D3D_OK);
        if (FAILED(hr)) continue;

        for (j = 0; j < test_data[i].num_faces * 3; j++)
            ok(adjacency[j] == test_data[i].adjacency[j],
               "Test %d adjacency %d: Got result %u, expected %u\n", i, j,
               adjacency[j], test_data[i].adjacency[j]);
    }
    if (d3dxmesh) d3dxmesh->lpVtbl->Release(d3dxmesh);
}

START_TEST(mesh)
{
    D3DXBoundProbeTest();
    D3DXComputeBoundingBoxTest();
    D3DXComputeBoundingSphereTest();
    D3DXGetFVFVertexSizeTest();
    D3DXIntersectTriTest();
    D3DXCreateMeshTest();
    D3DXCreateMeshFVFTest();
    D3DXCreateBoxTest();
    D3DXCreateSphereTest();
    D3DXCreateCylinderTest();
    D3DXCreateTextTest();
    test_get_decl_length();
    test_get_decl_vertex_size();
    test_fvf_decl_conversion();
    D3DXGenerateAdjacencyTest();
}
