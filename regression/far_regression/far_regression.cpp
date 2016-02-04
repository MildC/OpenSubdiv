//
//   Copyright 2013 Pixar
//
//   Licensed under the Apache License, Version 2.0 (the "Apache License")
//   with the following modification; you may not use this file except in
//   compliance with the Apache License and the following modification to it:
//   Section 6. Trademarks. is deleted and replaced with:
//
//   6. Trademarks. This License does not grant permission to use the trade
//      names, trademarks, service marks, or product names of the Licensor
//      and its affiliates, except as required to comply with Section 4(c) of
//      the License and to reproduce the content of the NOTICE file.
//
//   You may obtain a copy of the Apache License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the Apache License with the above modification is
//   distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
//   KIND, either express or implied. See the Apache License for the specific
//   language governing permissions and limitations under the Apache License.
//

#include <cassert>
#include <cstdio>


#include "../../regression/common/hbr_utils.h"
#include "../../regression/common/far_utils.h"
#include "../../regression/common/cmp_utils.h"

#include "init_shapes.h"

//
// Regression testing matching Far to Hbr (default CPU implementation)
//
// Notes:
// - precision is currently held at 1e-6
//
// - results cannot be bitwise identical as some vertex interpolations
//   are not happening in the same order.
//
// - only vertex interpolation is being tested at the moment.
//
#define PRECISION 1e-6

static bool g_debugmode = false;

//------------------------------------------------------------------------------
// Vertex class implementation
struct xyzVV {

    xyzVV() { /* _pos[0]=_pos[1]=_pos[2]=0.0f; */ }

    xyzVV( int /*i*/ ) { }

    xyzVV( float x, float y, float z ) { _pos[0]=x; _pos[1]=y; _pos[2]=z; }

    xyzVV( const xyzVV & src ) { _pos[0]=src._pos[0]; _pos[1]=src._pos[1]; _pos[2]=src._pos[2]; }

   ~xyzVV( ) { }

    void AddWithWeight(const xyzVV& src, float weight) {
        _pos[0]+=weight*src._pos[0];
        _pos[1]+=weight*src._pos[1];
        _pos[2]+=weight*src._pos[2];
    }

    void AddVaryingWithWeight(const xyzVV& , float) { }

    void Clear( void * =0 ) { _pos[0]=_pos[1]=_pos[2]=0.0f; }

    void SetPosition(float x, float y, float z) { _pos[0]=x; _pos[1]=y; _pos[2]=z; }

    void ApplyVertexEdit(const OpenSubdiv::HbrVertexEdit<xyzVV> & edit) {
        const float *src = edit.GetEdit();
        switch(edit.GetOperation()) {
          case OpenSubdiv::HbrHierarchicalEdit<xyzVV>::Set:
            _pos[0] = src[0];
            _pos[1] = src[1];
            _pos[2] = src[2];
            break;
          case OpenSubdiv::HbrHierarchicalEdit<xyzVV>::Add:
            _pos[0] += src[0];
            _pos[1] += src[1];
            _pos[2] += src[2];
            break;
          case OpenSubdiv::HbrHierarchicalEdit<xyzVV>::Subtract:
            _pos[0] -= src[0];
            _pos[1] -= src[1];
            _pos[2] -= src[2];
            break;
        }
    }

    void ApplyMovingVertexEdit(const OpenSubdiv::HbrMovingVertexEdit<xyzVV> &) { }

    const float * GetPos() const { return _pos; }

    bool operator==(xyzVV const & other) const {
        if (_pos[0]==other._pos[0] and
            _pos[1]==other._pos[1] and
            _pos[2]==other._pos[2]) {
            return true;
        }
        return false;
    }

private:
    float _pos[3];
};

//------------------------------------------------------------------------------
typedef OpenSubdiv::HbrMesh<xyzVV>           Hmesh;

//------------------------------------------------------------------------------
typedef OpenSubdiv::Far::TopologyRefiner               FarTopologyRefiner;
typedef OpenSubdiv::Far::TopologyRefinerFactory<Shape> FarTopologyRefinerFactory;

//------------------------------------------------------------------------------
#ifdef foo
static void
printVertexData(std::vector<xyzVV> const & hbrBuffer, std::vector<xyzVV> const & farBuffer) {

    assert(hbrBuffer.size()==farBuffer.size());
    for (int i=0; i<(int)hbrBuffer.size(); ++i) {

        float const * hbr = hbrBuffer[i].GetPos(),
                    * far = farBuffer[i].GetPos();

        printf("%3d %d (%f %f %f) (%f %f %f)\n", i, hbrBuffer[i]==farBuffer[i],
                                                    hbr[0], hbr[1], hbr[2],
                                                    far[0], far[1], far[2]);
    }
}
#endif

//------------------------------------------------------------------------------
static int
checkMesh(ShapeDesc const & desc, int maxlevel) {

    static char const * schemes[] = { "Bilinear", "Catmark", "Loop" };
    printf("- %-25s ( %-8s ): \n", desc.name.c_str(), schemes[desc.scheme]);

    int count=0;
    float deltaAvg[3] = {0.0f, 0.0f, 0.0f},
          deltaCnt[3] = {0.0f, 0.0f, 0.0f};

    std::vector<xyzVV> hbrVertexData,
                       farVertexData;

    Hmesh *  hmesh = interpolateHbrVertexData<xyzVV>(
        desc.data.c_str(), desc.scheme, maxlevel);

    FarTopologyRefiner * refiner =
        InterpolateFarVertexData<xyzVV>(
            desc.data.c_str(), desc.scheme, maxlevel, farVertexData);

    // copy Hbr vertex data into a re-ordered buffer (for easier comparison)
    GetReorderedHbrVertexData(*refiner, *hmesh, &hbrVertexData);

    int nverts = (int)farVertexData.size();

    for (int i=0; i<nverts; ++i) {

        xyzVV & hbrVert = hbrVertexData[i],
              & farVert = farVertexData[i];

#ifdef __INTEL_COMPILER // remark #1572: floating-point equality and inequality comparisons are unreliable
#pragma warning disable 1572
#endif
        if ( hbrVert.GetPos()[0] != farVert.GetPos()[0] )
            deltaCnt[0]++;
        if ( hbrVert.GetPos()[1] != farVert.GetPos()[1] )
            deltaCnt[1]++;
        if ( hbrVert.GetPos()[2] != farVert.GetPos()[2] )
            deltaCnt[2]++;
#ifdef __INTEL_COMPILER
#pragma warning enable 1572
#endif
        float delta[3] = { hbrVert.GetPos()[0] - farVert.GetPos()[0],
                           hbrVert.GetPos()[1] - farVert.GetPos()[1],
                           hbrVert.GetPos()[2] - farVert.GetPos()[2] };

        deltaAvg[0]+=delta[0];
        deltaAvg[1]+=delta[1];
        deltaAvg[2]+=delta[2];

        float dist = sqrtf( delta[0]*delta[0]+delta[1]*delta[1]+delta[2]*delta[2]);
        if ( dist > PRECISION ) {
            if (not g_debugmode)
                printf("// HbrVertex<T> %d fails : dist=%.10f (%.10f %.10f %.10f)"
                       " (%.10f %.10f %.10f)\n", i, dist, hbrVert.GetPos()[0],
                                                          hbrVert.GetPos()[1],
                                                          hbrVert.GetPos()[2],
                                                          farVert.GetPos()[0],
                                                          farVert.GetPos()[1],
                                                          farVert.GetPos()[2] );
           count++;
        }
    }

    if (deltaCnt[0])
        deltaAvg[0]/=deltaCnt[0];
    if (deltaCnt[1])
        deltaAvg[1]/=deltaCnt[1];
    if (deltaCnt[2])
        deltaAvg[2]/=deltaCnt[2];

    if (not g_debugmode) {
        printf("  delta ratio : (%d/%d %d/%d %d/%d)\n", (int)deltaCnt[0], nverts,
                                                        (int)deltaCnt[1], nverts,
                                                        (int)deltaCnt[2], nverts );
        printf("  average delta : (%.10f %.10f %.10f)\n", deltaAvg[0],
                                                          deltaAvg[1],
                                                          deltaAvg[2] );
        if (count==0)
            printf("  success !\n");
    }

    return count;
}

//------------------------------------------------------------------------------
int main(int /* argc */, char ** /* argv */) {

    int levels=5, total=0;

    initShapes();

    if (g_debugmode)
        printf("[ ");
    else
        printf("precision : %f\n",PRECISION);
    for (int i=0; i<(int)g_shapes.size(); ++i) {
        total+=checkMesh(g_shapes[i], levels);
    }

    if (g_debugmode)
        printf("]\n");
    else {
        if (total==0)
          printf("All tests passed.\n");
        else
          printf("Total failures : %d\n", total);
    }
}

//------------------------------------------------------------------------------
