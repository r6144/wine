@ stdcall D3DXAssembleShader(ptr long ptr ptr long ptr ptr)
@ stdcall D3DXAssembleShaderFromFileA(str ptr ptr long ptr ptr)
@ stdcall D3DXAssembleShaderFromFileW(wstr ptr ptr long ptr ptr)
@ stdcall D3DXAssembleShaderFromResourceA(long str ptr ptr long ptr ptr)
@ stdcall D3DXAssembleShaderFromResourceW(long wstr ptr ptr long ptr ptr)
@ stdcall D3DXBoxBoundProbe(ptr ptr ptr ptr)
@ stdcall D3DXCheckCubeTextureRequirements(ptr ptr ptr long ptr ptr)
@ stdcall D3DXCheckTextureRequirements(ptr ptr ptr ptr long ptr ptr)
@ stdcall D3DXCheckVersion(long long)
@ stdcall D3DXCheckVolumeTextureRequirements(ptr ptr ptr ptr ptr long ptr ptr)
@ stub D3DXCleanMesh
@ stdcall D3DXColorAdjustContrast(ptr ptr float)
@ stdcall D3DXColorAdjustSaturation(ptr ptr float)
@ stdcall D3DXCompileShader(ptr long ptr ptr str str long ptr ptr ptr)
@ stdcall D3DXCompileShaderFromFileA(str ptr ptr str str long ptr ptr ptr)
@ stdcall D3DXCompileShaderFromFileW(wstr ptr ptr str str long ptr ptr ptr)
@ stdcall D3DXCompileShaderFromResourceA(ptr str ptr ptr str str long ptr ptr ptr)
@ stdcall D3DXCompileShaderFromResourceW(ptr wstr ptr ptr str str long ptr ptr ptr)
@ stdcall D3DXComputeBoundingBox(ptr long long ptr ptr)
@ stdcall D3DXComputeBoundingSphere(ptr long long ptr ptr)
@ stub D3DXComputeIMTFromPerTexelSignal
@ stub D3DXComputeIMTFromPerVertexSignal
@ stub D3DXComputeIMTFromSignal
@ stub D3DXComputeIMTFromTexture
@ stub D3DXComputeNormalMap
@ stub D3DXComputeNormals
@ stub D3DXComputeTangent
@ stub D3DXComputeTangentFrame
@ stub D3DXComputeTangentFrameEx
@ stub D3DXConcatenateMeshes
@ stub D3DXConvertMeshSubsetToSingleStrip
@ stub D3DXConvertMeshSubsetToStrips
@ stub D3DXCreateAnimationController
@ stdcall D3DXCreateBox(ptr float float float ptr ptr)
@ stdcall D3DXCreateBuffer(long ptr)
@ stub D3DXCreateCompressedAnimationSet
@ stdcall D3DXCreateCubeTexture(ptr long long long long long ptr)
@ stub D3DXCreateCubeTextureFromFileA
@ stub D3DXCreateCubeTextureFromFileExA
@ stub D3DXCreateCubeTextureFromFileExW
@ stub D3DXCreateCubeTextureFromFileInMemory
@ stub D3DXCreateCubeTextureFromFileInMemoryEx
@ stub D3DXCreateCubeTextureFromFileW
@ stub D3DXCreateCubeTextureFromResourceA
@ stub D3DXCreateCubeTextureFromResourceExA
@ stub D3DXCreateCubeTextureFromResourceExW
@ stub D3DXCreateCubeTextureFromResourceW
@ stdcall D3DXCreateCylinder(ptr long long long long long ptr ptr)
@ stdcall D3DXCreateEffect(ptr ptr long ptr ptr long ptr ptr ptr)
@ stdcall D3DXCreateEffectCompiler(ptr long ptr ptr long ptr ptr)
@ stdcall D3DXCreateEffectCompilerFromFileA(str ptr ptr long ptr ptr)
@ stdcall D3DXCreateEffectCompilerFromFileW(wstr ptr ptr long ptr ptr)
@ stdcall D3DXCreateEffectCompilerFromResourceA(long str ptr ptr long ptr ptr)
@ stdcall D3DXCreateEffectCompilerFromResourceW(long wstr ptr ptr long ptr ptr)
@ stdcall D3DXCreateEffectEx(ptr ptr long ptr ptr str long ptr ptr ptr)
@ stdcall D3DXCreateEffectFromFileA(ptr str ptr ptr long ptr ptr ptr)
@ stdcall D3DXCreateEffectFromFileExA(ptr str ptr ptr str long ptr ptr ptr)
@ stdcall D3DXCreateEffectFromFileExW(ptr str ptr ptr str long ptr ptr ptr)
@ stdcall D3DXCreateEffectFromFileW(ptr wstr ptr ptr long ptr ptr ptr)
@ stdcall D3DXCreateEffectFromResourceA(ptr long str ptr ptr long ptr ptr ptr)
@ stdcall D3DXCreateEffectFromResourceExA(ptr long str ptr ptr str long ptr ptr ptr)
@ stdcall D3DXCreateEffectFromResourceExW(ptr long str ptr ptr str long ptr ptr ptr)
@ stdcall D3DXCreateEffectFromResourceW(ptr long wstr ptr ptr long ptr ptr ptr)
@ stdcall D3DXCreateEffectPool(ptr)
@ stdcall D3DXCreateFontA(ptr long long long long long long long long long str ptr)
@ stdcall D3DXCreateFontIndirectA(ptr ptr ptr)
@ stdcall D3DXCreateFontIndirectW(ptr ptr ptr)
@ stdcall D3DXCreateFontW(ptr long long long long long long long long long wstr ptr)
@ stub D3DXCreateFragmentLinker
@ stub D3DXCreateFragmentLinkerEx
@ stub D3DXCreateKeyframedAnimationSet
@ stdcall D3DXCreateLine(ptr ptr)
@ stdcall D3DXCreateMatrixStack(long ptr)
@ stdcall D3DXCreateMesh(long long long ptr ptr ptr)
@ stdcall D3DXCreateMeshFVF(long long long long ptr ptr)
@ stub D3DXCreateNPatchMesh
@ stub D3DXCreatePMeshFromStream
@ stub D3DXCreatePatchMesh
@ stub D3DXCreatePolygon
@ stub D3DXCreatePRTBuffer
@ stub D3DXCreatePRTBufferTex
@ stub D3DXCreatePRTCompBuffer
@ stub D3DXCreatePRTEngine
@ stub D3DXCreateRenderToEnvMap
@ stub D3DXCreateRenderToSurface
@ stub D3DXCreateSPMesh
@ stub D3DXCreateSkinInfo
@ stub D3DXCreateSkinInfoFromBlendedMesh
@ stub D3DXCreateSkinInfoFVF
@ stdcall D3DXCreateSphere(ptr float long long ptr ptr)
@ stdcall D3DXCreateSprite(ptr ptr)
@ stdcall D3DXCreateTeapot(ptr ptr ptr)
@ stub D3DXCreateTextA
@ stub D3DXCreateTextW
@ stdcall D3DXCreateTexture(ptr long long long long long long ptr)
@ stdcall D3DXCreateTextureFromFileA(ptr str ptr)
@ stdcall D3DXCreateTextureFromFileExA(ptr str long long long long long long long long long ptr ptr ptr)
@ stdcall D3DXCreateTextureFromFileExW(ptr wstr long long long long long long long long long ptr ptr ptr)
@ stdcall D3DXCreateTextureFromFileInMemory(ptr ptr long ptr)
@ stdcall D3DXCreateTextureFromFileInMemoryEx(ptr ptr long long long long long long long long long long ptr ptr ptr)
@ stdcall D3DXCreateTextureFromFileW(ptr wstr ptr)
@ stdcall D3DXCreateTextureFromResourceA(ptr ptr str ptr)
@ stdcall D3DXCreateTextureFromResourceExA(ptr ptr str long long long long long long long long long ptr ptr ptr)
@ stdcall D3DXCreateTextureFromResourceExW(ptr ptr wstr long long long long long long long long long ptr ptr ptr)
@ stdcall D3DXCreateTextureFromResourceW(ptr ptr wstr ptr)
@ stub D3DXCreateTextureGutterHelper
@ stub D3DXCreateTextureShader
@ stub D3DXCreateTorus
@ stdcall D3DXCreateVolumeTexture(ptr long long long long long long long ptr)
@ stub D3DXCreateVolumeTextureFromFileA
@ stub D3DXCreateVolumeTextureFromFileExA
@ stub D3DXCreateVolumeTextureFromFileExW
@ stub D3DXCreateVolumeTextureFromFileInMemory
@ stub D3DXCreateVolumeTextureFromFileInMemoryEx
@ stub D3DXCreateVolumeTextureFromFileW
@ stub D3DXCreateVolumeTextureFromResourceA
@ stub D3DXCreateVolumeTextureFromResourceExA
@ stub D3DXCreateVolumeTextureFromResourceExW
@ stub D3DXCreateVolumeTextureFromResourceW
@ stdcall D3DXDebugMute(long)
@ stdcall D3DXDeclaratorFromFVF(long ptr)
@ stub D3DXDisassembleEffect
@ stub D3DXDisassembleShader
@ stub D3DXFileCreate
@ stdcall D3DXFillCubeTexture(ptr ptr ptr)
@ stub D3DXFillCubeTextureTX
@ stdcall D3DXFillTexture(ptr ptr ptr)
@ stub D3DXFillTextureTX
@ stdcall D3DXFillVolumeTexture(ptr ptr ptr)
@ stub D3DXFillVolumeTextureTX
@ stdcall D3DXFilterTexture(ptr ptr long long)
@ stdcall D3DXFindShaderComment(ptr long ptr ptr)
@ stub D3DXFloat16To32Array
@ stub D3DXFloat32To16Array
@ stub D3DXFrameAppendChild
@ stub D3DXFrameCalculateBoundingSphere
@ stub D3DXFrameDestroy
@ stub D3DXFrameFind
@ stub D3DXFrameNumNamedMatrices
@ stub D3DXFrameRegisterNamedMatrices
@ stdcall D3DXFresnelTerm(float float)
@ stdcall D3DXFVFFromDeclarator(ptr ptr)
@ stub D3DXGatherFragments
@ stub D3DXGatherFragmentsFromFileA
@ stub D3DXGatherFragmentsFromFileW
@ stub D3DXGatherFragmentsFromResourceA
@ stub D3DXGatherFragmentsFromResourceW
@ stub D3DXGenerateOutputDecl
@ stub D3DXGeneratePMesh
@ stdcall D3DXGetDeclLength(ptr)
@ stdcall D3DXGetDeclVertexSize(ptr long)
@ stdcall D3DXGetDriverLevel(ptr)
@ stdcall D3DXGetFVFVertexSize(long)
@ stdcall D3DXGetImageInfoFromFileA(str ptr)
@ stdcall D3DXGetImageInfoFromFileInMemory(ptr long ptr)
@ stdcall D3DXGetImageInfoFromFileW(wstr ptr)
@ stdcall D3DXGetImageInfoFromResourceA(long str ptr)
@ stdcall D3DXGetImageInfoFromResourceW(long wstr ptr)
@ stdcall D3DXGetPixelShaderProfile(ptr)
@ stdcall D3DXGetShaderConstantTable(ptr ptr)
@ stdcall D3DXGetShaderConstantTableEx(ptr long ptr)
@ stub D3DXGetShaderInputSemantics
@ stub D3DXGetShaderOutputSemantics
@ stub D3DXGetShaderSamplers
@ stdcall D3DXGetShaderSize(ptr)
@ stdcall D3DXGetShaderVersion(ptr)
@ stdcall D3DXGetVertexShaderProfile(ptr)
@ stub D3DXIntersect
@ stub D3DXIntersectSubset
@ stdcall D3DXIntersectTri(ptr ptr ptr ptr ptr ptr ptr ptr)
@ stub D3DXLoadMeshFromXA
@ stub D3DXLoadMeshFromXInMemory
@ stub D3DXLoadMeshFromXResource
@ stub D3DXLoadMeshFromXW
@ stub D3DXLoadMeshFromXof
@ stub D3DXLoadMeshHierarchyFromXA
@ stub D3DXLoadMeshHierarchyFromXInMemory
@ stub D3DXLoadMeshHierarchyFromXW
@ stub D3DXLoadPatchMeshFromXof
@ stub D3DXLoadPRTBufferFromFileA
@ stub D3DXLoadPRTBufferFromFileW
@ stub D3DXLoadPRTCompBufferFromFileA
@ stub D3DXLoadPRTCompBufferFromFileW
@ stub D3DXLoadSkinMeshFromXof
@ stdcall D3DXLoadSurfaceFromFileA(ptr ptr ptr str ptr long long ptr)
@ stdcall D3DXLoadSurfaceFromFileInMemory(ptr ptr ptr ptr long ptr long long ptr)
@ stdcall D3DXLoadSurfaceFromFileW(ptr ptr ptr wstr ptr long long ptr)
@ stdcall D3DXLoadSurfaceFromMemory(ptr ptr ptr ptr long long ptr ptr long long)
@ stdcall D3DXLoadSurfaceFromResourceA(ptr ptr ptr ptr str ptr long long ptr)
@ stdcall D3DXLoadSurfaceFromResourceW(ptr ptr ptr ptr wstr ptr long long ptr)
@ stdcall D3DXLoadSurfaceFromSurface(ptr ptr ptr ptr ptr ptr long long)
@ stub D3DXLoadVolumeFromFileA
@ stub D3DXLoadVolumeFromFileInMemory
@ stub D3DXLoadVolumeFromFileW
@ stdcall D3DXLoadVolumeFromMemory(ptr ptr ptr ptr long long long ptr ptr long long)
@ stub D3DXLoadVolumeFromResourceA
@ stub D3DXLoadVolumeFromResourceW
@ stub D3DXLoadVolumeFromVolume
@ stdcall D3DXMatrixAffineTransformation(ptr float ptr ptr ptr)
@ stdcall D3DXMatrixAffineTransformation2D(ptr float ptr float ptr)
@ stdcall D3DXMatrixDecompose(ptr ptr ptr ptr)
@ stdcall D3DXMatrixDeterminant(ptr)
@ stdcall D3DXMatrixInverse(ptr ptr ptr)
@ stdcall D3DXMatrixLookAtLH(ptr ptr ptr ptr)
@ stdcall D3DXMatrixLookAtRH(ptr ptr ptr ptr)
@ stdcall D3DXMatrixMultiply(ptr ptr ptr)
@ stdcall D3DXMatrixMultiplyTranspose(ptr ptr ptr)
@ stdcall D3DXMatrixOrthoLH(ptr float float float float)
@ stdcall D3DXMatrixOrthoOffCenterLH(ptr float float float float float float)
@ stdcall D3DXMatrixOrthoOffCenterRH(ptr float float float float float float)
@ stdcall D3DXMatrixOrthoRH(ptr float float float float)
@ stdcall D3DXMatrixPerspectiveFovLH(ptr float float float float)
@ stdcall D3DXMatrixPerspectiveFovRH(ptr float float float float)
@ stdcall D3DXMatrixPerspectiveLH(ptr float float float float)
@ stdcall D3DXMatrixPerspectiveOffCenterLH(ptr float float float float float float)
@ stdcall D3DXMatrixPerspectiveOffCenterRH(ptr float float float float float float)
@ stdcall D3DXMatrixPerspectiveRH(ptr float float float float)
@ stdcall D3DXMatrixReflect(ptr ptr)
@ stdcall D3DXMatrixRotationAxis(ptr ptr float)
@ stdcall D3DXMatrixRotationQuaternion(ptr ptr)
@ stdcall D3DXMatrixRotationX(ptr float)
@ stdcall D3DXMatrixRotationY(ptr float)
@ stdcall D3DXMatrixRotationYawPitchRoll(ptr float float float)
@ stdcall D3DXMatrixRotationZ(ptr float)
@ stdcall D3DXMatrixScaling(ptr float float float)
@ stdcall D3DXMatrixShadow(ptr ptr ptr)
@ stdcall D3DXMatrixTransformation(ptr ptr ptr ptr ptr ptr ptr)
@ stdcall D3DXMatrixTransformation2D(ptr ptr float ptr ptr float ptr)
@ stdcall D3DXMatrixTranslation(ptr float float float)
@ stdcall D3DXMatrixTranspose(ptr ptr)
@ stub D3DXOptimizeFaces
@ stub D3DXOptimizeVertices
@ stdcall D3DXPlaneFromPointNormal(ptr ptr ptr)
@ stdcall D3DXPlaneFromPoints(ptr ptr ptr ptr)
@ stdcall D3DXPlaneIntersectLine(ptr ptr ptr ptr)
@ stdcall D3DXPlaneNormalize(ptr ptr)
@ stdcall D3DXPlaneTransform(ptr ptr ptr)
@ stdcall D3DXPlaneTransformArray(ptr long ptr long ptr long)
@ stdcall D3DXPreprocessShader(ptr long ptr ptr ptr ptr)
@ stdcall D3DXPreprocessShaderFromFileA(str ptr ptr ptr ptr)
@ stdcall D3DXPreprocessShaderFromFileW(wstr ptr ptr ptr ptr)
@ stdcall D3DXPreprocessShaderFromResourceA(long str ptr ptr ptr ptr)
@ stdcall D3DXPreprocessShaderFromResourceW(long wstr ptr ptr ptr ptr)
@ stdcall D3DXQuaternionBaryCentric(ptr ptr ptr ptr float float)
@ stdcall D3DXQuaternionExp(ptr ptr)
@ stdcall D3DXQuaternionInverse(ptr ptr)
@ stdcall D3DXQuaternionLn(ptr ptr)
@ stdcall D3DXQuaternionMultiply(ptr ptr ptr)
@ stdcall D3DXQuaternionNormalize(ptr ptr)
@ stdcall D3DXQuaternionRotationAxis(ptr ptr float)
@ stdcall D3DXQuaternionRotationMatrix(ptr ptr)
@ stdcall D3DXQuaternionRotationYawPitchRoll(ptr float float float)
@ stdcall D3DXQuaternionSlerp(ptr ptr ptr float)
@ stdcall D3DXQuaternionSquad(ptr ptr ptr ptr ptr float)
@ stub D3DXQuaternionSquadSetup
@ stdcall D3DXQuaternionToAxisAngle(ptr ptr ptr)
@ stub D3DXRectPatchSize
@ stub D3DXSaveMeshHierarchyToFileA
@ stub D3DXSaveMeshHierarchyToFileW
@ stub D3DXSaveMeshToXA
@ stub D3DXSaveMeshToXW
@ stub D3DXSavePRTBufferToFileA
@ stub D3DXSavePRTBufferToFileW
@ stub D3DXSavePRTCompBufferToFileA
@ stub D3DXSavePRTCompBufferToFileW
@ stub D3DXSaveSurfaceToFileA
@ stub D3DXSaveSurfaceToFileInMemory
@ stub D3DXSaveSurfaceToFileW
@ stub D3DXSaveTextureToFileA
@ stub D3DXSaveTextureToFileInMemory
@ stub D3DXSaveTextureToFileW
@ stub D3DXSaveVolumeToFileA
@ stub D3DXSaveVolumeToFileInMemory
@ stub D3DXSaveVolumeToFileW
@ stub D3DXSHAdd
@ stub D3DXSHDot
@ stub D3DXSHEvalConeLight
@ stub D3DXSHEvalDirection
@ stub D3DXSHEvalDirectionalLight
@ stub D3DXSHEvalHemisphereLight
@ stub D3DXSHEvalSphericalLight
@ stub D3DXSHMultiply2
@ stub D3DXSHMultiply3
@ stub D3DXSHMultiply4
@ stub D3DXSHMultiply5
@ stub D3DXSHMultiply6
@ stub D3DXSHProjectCubeMap
@ stub D3DXSHPRTCompSplitMeshSC
@ stub D3DXSHPRTCompSuperCluster
@ stub D3DXSHRotate
@ stub D3DXSHRotateZ
@ stub D3DXSHScale
@ stub D3DXSimplifyMesh
@ stdcall D3DXSphereBoundProbe(ptr float ptr ptr)
@ stub D3DXSplitMesh
@ stub D3DXTessellateNPatches
@ stub D3DXTessellateRectPatch
@ stub D3DXTessellateTriPatch
@ stub D3DXTriPatchSize
@ stub D3DXUVAtlasCreate
@ stub D3DXUVAtlasPack
@ stub D3DXUVAtlasPartition
@ stub D3DXValidMesh
@ stub D3DXValidPatchMesh
@ stdcall D3DXVec2BaryCentric(ptr ptr ptr ptr float float)
@ stdcall D3DXVec2CatmullRom(ptr ptr ptr ptr ptr float)
@ stdcall D3DXVec2Hermite(ptr ptr ptr ptr ptr float)
@ stdcall D3DXVec2Normalize(ptr ptr)
@ stdcall D3DXVec2Transform(ptr ptr ptr)
@ stdcall D3DXVec2TransformArray(ptr long ptr long ptr long)
@ stdcall D3DXVec2TransformCoord(ptr ptr ptr)
@ stdcall D3DXVec2TransformCoordArray(ptr long ptr long ptr long)
@ stdcall D3DXVec2TransformNormal(ptr ptr ptr)
@ stdcall D3DXVec2TransformNormalArray(ptr long ptr long ptr long)
@ stdcall D3DXVec3BaryCentric(ptr ptr ptr ptr float float)
@ stdcall D3DXVec3CatmullRom(ptr ptr ptr ptr ptr float)
@ stdcall D3DXVec3Hermite(ptr ptr ptr ptr ptr float)
@ stdcall D3DXVec3Normalize(ptr ptr)
@ stdcall D3DXVec3Project(ptr ptr ptr ptr ptr ptr)
@ stdcall D3DXVec3ProjectArray(ptr long ptr long ptr ptr ptr ptr long)
@ stdcall D3DXVec3Transform(ptr ptr ptr)
@ stdcall D3DXVec3TransformArray(ptr long ptr long ptr long)
@ stdcall D3DXVec3TransformCoord(ptr ptr ptr)
@ stdcall D3DXVec3TransformCoordArray(ptr long ptr long ptr long)
@ stdcall D3DXVec3TransformNormal(ptr ptr ptr)
@ stdcall D3DXVec3TransformNormalArray(ptr long ptr long ptr long)
@ stdcall D3DXVec3Unproject(ptr ptr ptr ptr ptr ptr)
@ stdcall D3DXVec3UnprojectArray(ptr long ptr long ptr ptr ptr ptr long)
@ stdcall D3DXVec4BaryCentric(ptr ptr ptr ptr float float)
@ stdcall D3DXVec4CatmullRom(ptr ptr ptr ptr ptr float)
@ stdcall D3DXVec4Cross(ptr ptr ptr ptr)
@ stdcall D3DXVec4Hermite(ptr ptr ptr ptr ptr float)
@ stdcall D3DXVec4Normalize(ptr ptr)
@ stdcall D3DXVec4Transform(ptr ptr ptr)
@ stdcall D3DXVec4TransformArray(ptr long ptr long ptr long)
@ stub D3DXWeldVertices
