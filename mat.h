#pragma once
#include "mgs/common/util.h"
#include "mgs/archive/dar/dar.h"
#include "image/pcx/dr_pcx.h"

inline
int findMaterialIdx(char* matName, CArrayList<noesisMaterial_t*>& matList) {
    for (int i = 0; i < matList.Num(); i++) {
        if (!strcmp(matList[i]->name, matName))
            return i;
    }
    return -1;
}

inline
int findTextureIdx(char* texName, CArrayList<noesisTex_t*>& texList) {
    for (int i = 0; i < texList.Num(); i++) {
        if (!strcmp(texList[i]->filename, texName))
            return i;
    }
    return -1;
}

inline
uint8_t* findPcx(noeRAPI_t* rapi, uint16_t& strcode, int& size) {
    std::filesystem::path p{ rapi->Noesis_GetInputName() };
    p = p.parent_path();

    for (const std::filesystem::directory_entry& file : std::filesystem::recursive_directory_iterator(p)) {
        if (file.path().extension() == ".dar") {
            Dar dar = Dar(file.path().u8string());

            if (uint8_t* pcx = dar.findFile(strcode, 0x70, size))
                return pcx;
        }
    }

    return NULL;
}

inline
noesisTex_t* loadTexture(noeRAPI_t* rapi, uint16_t& strcode, noesisTex_t **alphaTexture) {
    int size;
    uint8_t* texData = findPcx(rapi, strcode, size);
    if (!texData) return NULL;

    int width;
    int height;
    int components;

    drpcx pcxResult{};
    int loadResult = drpcx_load_memory(&pcxResult, texData, size, false, &width, &height, &components, 4);
    if (loadResult <= 0 || pcxResult.loaded <= 0) {
        char s[64];
        sprintf(s, "Can't load image %04X", strcode);
        MessageBoxA(NULL, s, "Error", 0);
        return NULL;
    }

    uint8_t* imageData = pcxResult.pImageData;
    uint8_t* paletteData = pcxResult.pPaletteIndices;

    int datasize = width * height * 4;
    uint8_t* tga = makeTGA(imageData, datasize, width, height);
    uint8_t* alphaTga = makeTGAPalette(paletteData, datasize, width, height);
    
    drpcx_free(imageData);
    drpcx_free(paletteData);

    noesisTex_t* noeTexture = rapi->Noesis_LoadTexByHandler(tga, datasize + 0x12, ".tga");
    noesisTex_t* noeTextureAlpha = rapi->Noesis_LoadTexByHandler(alphaTga, datasize + 0x12, ".tga");
    if (alphaTexture != nullptr) {
        *alphaTexture = noeTextureAlpha;
    }

    delete[] tga;
    delete[] alphaTga;
    delete[] texData;
    return noeTexture;
}

inline
void bindMat(uint16_t strcode, BYTE* fileBuffer, noeRAPI_t* rapi, CArrayList<noesisMaterial_t*>& matList, CArrayList<noesisTex_t*>& texList) {

    //set mat name
    std::string matStr = intToHexString(strcode);
    char matName[7];
    strcpy_s(matName, matStr.c_str());

    //check if material already exists
    int x = findMaterialIdx(matName, matList);

    //use existing mat if it does
    if (x > -1) {
        rapi->rpgSetMaterial(matName);
        return;
    }
    
    //create material
    noesisMaterial_t* noeMat = rapi->Noesis_GetMaterialList(1, false);
    noeMat->name = rapi->Noesis_PooledString(matName);
    noeMat->noLighting = true;

    //set tex name
    std::string texStr = intToHexString(strcode) + ".tga";
    std::string texStrA = intToHexString(strcode) + "_a.tga";

    char texName[32];
    char texNameAlpha[32];
    strcpy_s(texName, texStr.c_str());
    strcpy_s(texNameAlpha, texStrA.c_str());

    //check if texture already exists
    int y = findTextureIdx(texName, texList);

    //set tex to mat if it does
    if (y > -1) {
        noeMat->texIdx = y;
        matList.Append(noeMat);
        rapi->rpgSetMaterial(matName);
        return;
    }

    //load texture
    noesisTex_t* alphaMask = nullptr;
    noesisTex_t* noeTexture = loadTexture(rapi, strcode, &alphaMask);
    
    if (!noeTexture || !alphaMask) return;

    noeTexture->filename = rapi->Noesis_PooledString(texName);
    alphaMask->filename = rapi->Noesis_PooledString(texNameAlpha);

    //set tex to mat
    noeMat->texIdx = texList.Num();
    matList.Append(noeMat);
    
    texList.Append(noeTexture);
    texList.Append(alphaMask);

    //set material
    rapi->rpgSetMaterial(noeMat->name);
}