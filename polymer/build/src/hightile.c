/*
 * High-colour textures support for Polymost
 * by Jonathon Fowler
 * See the included license file "BUILDLIC.TXT" for license info.
 */

#include "kplib.h"

#define HICEFFECTMASK (1|2|4)
palette_t hictinting[MAXPALOOKUPS];

//moved into polymost.h
/*struct hicskybox_t {
    int ignore;
    char *face[6];
};

typedef struct hicreplc_t {
    struct hicreplc_t *next;
    char palnum, ignore, flags, filler;
    char *filename;
    float alphacut;
    struct hicskybox_t *skybox;
} hicreplctyp;*/
static hicreplctyp *hicreplc[MAXTILES];
static char hicfirstinit = 0;

//
// find the index into hicreplc[] which contains the replacement tile particulars
//
static hicreplctyp * hicfindsubst(int picnum, int palnum, int skybox)
{
    hicreplctyp *hr;

    if (!hicfirstinit) return NULL;
    if ((unsigned int)picnum >= (unsigned int)MAXTILES) return NULL;

    do
    {
        for (hr = hicreplc[picnum]; hr; hr = hr->next)
        {
            if (hr->palnum == palnum)
            {
                if (skybox)
                {
                    if (hr->skybox && !hr->skybox->ignore) return hr;
                }
                else
                {
                    if (!hr->ignore) return hr;
                }
            }
        }

        if (!palnum || palnum >= (MAXPALOOKUPS - RESERVEDPALS)) break;
        palnum = 0;
    }
    while (1);

    return NULL;	// no replacement found
}


//
// hicinit()
//   Initialize the high-colour stuff to default.
//
void hicinit(void)
{
    int i,j;
    hicreplctyp *hr, *next;

    clearconv();
    for (i=0;i<MAXPALOOKUPS;i++)  	// all tints should be 100%
    {
        hictinting[i].r = hictinting[i].g = hictinting[i].b = 0xff;
        hictinting[i].f = 0;
    }

    if (hicfirstinit)
        for (i=MAXTILES-1;i>=0;i--)
        {
            for (hr=hicreplc[i]; hr;)
            {
                next = hr->next;

                if (hr->skybox)
                {
                    for (j=5;j>=0;j--)
                    {
                        if (hr->skybox->face[j])
                        {
                            free(hr->skybox->face[j]);
                        }
                    }
                    free(hr->skybox);
                }
                if (hr->filename) free(hr->filename);
                free(hr);

                hr = next;
            }
        }
    memset(hicreplc,0,sizeof(hicreplc));

    hicfirstinit = 1;
}


//
// hicsetpalettetint(pal,r,g,b,effect)
//   The tinting values represent a mechanism for emulating the effect of global sector
//   palette shifts on true-colour textures and only true-colour textures.
//   effect bitset: 1 = greyscale, 2 = invert
//
void hicsetpalettetint(int palnum, unsigned char r, unsigned char g, unsigned char b, unsigned char effect)
{
    if ((unsigned int)palnum >= (unsigned int)MAXPALOOKUPS) return;
    if (!hicfirstinit) hicinit();

    hictinting[palnum].r = r;
    hictinting[palnum].g = g;
    hictinting[palnum].b = b;
    hictinting[palnum].f = effect & HICEFFECTMASK;
}


//
// hicsetsubsttex(picnum,pal,filen,alphacut)
//   Specifies a replacement graphic file for an ART tile.
//
int hicsetsubsttex(int picnum, int palnum, char *filen, float alphacut, float xscale, float yscale, char flags)
{
    hicreplctyp *hr, *hrn;

    if ((unsigned int)picnum >= (unsigned int)MAXTILES) return -1;
    if ((unsigned int)palnum >= (unsigned int)MAXPALOOKUPS) return -1;
    if (!hicfirstinit) hicinit();

    for (hr = hicreplc[picnum]; hr; hr = hr->next)
    {
        if (hr->palnum == palnum)
            break;
    }

    if (!hr)
    {
        // no replacement yet defined
        hrn = (hicreplctyp *)calloc(1,sizeof(hicreplctyp));
        if (!hrn) return -1;
        hrn->palnum = palnum;
    }
    else hrn = hr;

    // store into hicreplc the details for this replacement
    if (hrn->filename) free(hrn->filename);

    hrn->filename = strdup(filen);
    if (!hrn->filename)
    {
        if (hrn->skybox) return -1;	// don't free the base structure if there's a skybox defined
        if (hr == NULL) free(hrn);	// not yet a link in the chain
        return -1;
    }
    hrn->ignore = 0;
    hrn->alphacut = min(alphacut,1.0);
    hrn->xscale = xscale;
    hrn->yscale = yscale;
    hrn->flags = flags;
    if (hr == NULL)
    {
        hrn->next = hicreplc[picnum];
        hicreplc[picnum] = hrn;
    }

    //printf("Replacement [%d,%d]: %s\n", picnum, palnum, hicreplc[i]->filename);

    return 0;
}


//
// hicsetskybox(picnum,pal,faces[6])
//   Specifies a graphic files making up a skybox.
//
int hicsetskybox(int picnum, int palnum, char *faces[6])
{
    hicreplctyp *hr, *hrn;
    int j;

    if ((unsigned int)picnum >= (unsigned int)MAXTILES) return -1;
    if ((unsigned int)palnum >= (unsigned int)MAXPALOOKUPS) return -1;
    for (j=5;j>=0;j--) if (!faces[j]) return -1;
    if (!hicfirstinit) hicinit();

    for (hr = hicreplc[picnum]; hr; hr = hr->next)
    {
        if (hr->palnum == palnum)
            break;
    }

    if (!hr)
    {
        // no replacement yet defined
        hrn = (hicreplctyp *)calloc(1,sizeof(hicreplctyp));
        if (!hrn) return -1;

        hrn->palnum = palnum;
    }
    else hrn = hr;

    if (!hrn->skybox)
    {
        hrn->skybox = (struct hicskybox_t *)calloc(1,sizeof(struct hicskybox_t));
        if (!hrn->skybox)
        {
            if (hr == NULL) free(hrn);	// not yet a link in the chain
            return -1;
        }
    }
    else
    {
        for (j=5;j>=0;j--)
        {
            if (hrn->skybox->face[j])
                free(hrn->skybox->face[j]);
        }
    }

    // store each face's filename
    for (j=0;j<6;j++)
    {
        hrn->skybox->face[j] = strdup(faces[j]);
        if (!hrn->skybox->face[j])
        {
            for (--j; j>=0; --j)	// free any previous faces
                free(hrn->skybox->face[j]);
            free(hrn->skybox);
            hrn->skybox = NULL;
            if (hr == NULL) free(hrn);
            return -1;
        }
    }
    hrn->skybox->ignore = 0;
    if (hr == NULL)
    {
        hrn->next = hicreplc[picnum];
        hicreplc[picnum] = hrn;
    }

    return 0;
}


//
// hicclearsubst(picnum,pal)
//   Clears a replacement for an ART tile, including skybox faces.
//
int hicclearsubst(int picnum, int palnum)
{
    hicreplctyp *hr, *hrn = NULL;

    if ((unsigned int)picnum >= (unsigned int)MAXTILES) return -1;
    if ((unsigned int)palnum >= (unsigned int)MAXPALOOKUPS) return -1;
    if (!hicfirstinit) return 0;

    for (hr = hicreplc[picnum]; hr; hrn = hr, hr = hr->next)
    {
        if (hr->palnum == palnum)
            break;
    }

    if (!hr) return 0;

    if (hr->filename) free(hr->filename);
    if (hr->skybox)
    {
        int i;
        for (i=5;i>=0;i--)
            if (hr->skybox->face[i])
                free(hr->skybox->face[i]);
        free(hr->skybox);
    }

    if (hrn) hrn->next = hr->next;
    else hicreplc[picnum] = hr->next;
    free(hr);

    return 0;
}
