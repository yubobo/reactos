/*
 * PROJECT:         ReactOS win32 subsystem
 * LICENSE:         GPL - See COPYING in the top level directory
 * PURPOSE:         OpenType support for GDI font driver based on freetype
 * PROGRAMMER:      Timo Kreuzer (timo.kreuzer@reactos.org)
 * REFERENCES:      http://www.microsoft.com/typography/otspec/
 *                  http://www.microsoft.com/typography/otspec/os2.htm
 */

#include "ftfd.h"
#include <freetype/t1tables.h>

#define SWAPW(x) _byteswap_ushort(x)
#define SWAPD(x) _byteswap_ulong(x)

typedef struct _OTF_TABLE_ENTRY
{
    ULONG ulTag;
    ULONG ulCheckSum;
    ULONG ulOffset;
    ULONG ulLength;
} OTF_TABLE_ENTRY, *POTF_TABLE_ENTRY;

typedef struct _OTF_FILE_HEADER
{
    ULONG ulIdentifier;
    USHORT usNumTables;
    USHORT usSearchRange;
    USHORT usEntrySelector;
    USHORT usRangeshift;

    OTF_TABLE_ENTRY aTableEntries[1];

} OTF_FILE_HEADER, *POTF_FILE_HEADER;

#include <pshpack1.h>
typedef struct _OTF_OS2_DATA
{
    USHORT 	version; // 	0x0004
    SHORT 	xAvgCharWidth;
    USHORT 	usWeightClass;
    USHORT 	usWidthClass;
    USHORT 	fsType;
    SHORT 	ySubscriptXSize;
    SHORT 	ySubscriptYSize;
    SHORT 	ySubscriptXOffset;
    SHORT 	ySubscriptYOffset;
    SHORT 	ySuperscriptXSize;
    SHORT 	ySuperscriptYSize;
    SHORT 	ySuperscriptXOffset;
    SHORT 	ySuperscriptYOffset;
    SHORT 	yStrikeoutSize;
    SHORT 	yStrikeoutPosition;
    union
    {
        struct
        {
            BYTE jClassId;
            BYTE jSubClassId;
        };
        SHORT 	sFamilyClass;
    };
    BYTE 	panose[10];
    ULONG 	ulUnicodeRange1; // 	Bits 0-31
    ULONG 	ulUnicodeRange2; // 	Bits 32-63
    ULONG 	ulUnicodeRange3; // 	Bits 64-95
    ULONG 	ulUnicodeRange4; // 	Bits 96-127
    CHAR 	achVendID[4];
    USHORT 	fsSelection;
    USHORT 	usFirstCharIndex;
    USHORT 	usLastCharIndex;
    SHORT 	sTypoAscender;
    SHORT 	sTypoDescender;
    SHORT 	sTypoLineGap;
    USHORT 	usWinAscent;
    USHORT 	usWinDescent;
    ULONG 	ulCodePageRange1; // 	Bits 0-31
    ULONG 	ulCodePageRange2; // 	Bits 32-63
    SHORT 	sxHeight;
    SHORT 	sCapHeight;
    USHORT 	usDefaultChar;
    USHORT 	usBreakChar;
    USHORT 	usMaxContext;
} OTF_OS2_DATA, *POTF_OS2_DATA;
#include <poppack.h>

BOOL
OtfGetType1FontInfo(
    PFTFD_FACE pface,
    PIFIMETRICS pifi)
{
    FT_Error fterror;
    PS_FontInfoRec fontinfo;

    fterror = FT_Get_PS_Font_Info(pface->ftface, &fontinfo);
    if (fterror)
    {
        DbgPrint("ERROR: Failed to retrieve font info\n");
        return FALSE;
    }

    /* Try to find the font weight */
    if (strncpy(fontinfo.weight, "Normal", 7) == 0)
        pifi->usWinWeight = FW_NORMAL;
    // else if (..)
    else
        pifi->usWinWeight = FW_REGULAR;

    pifi->lItalicAngle = fontinfo.italic_angle;

    /* Clear lower 2 bits and set FIXED_PITCH or VARIABLE_PITCH */
    pifi->jWinPitchAndFamily &= ~3;
    if (fontinfo.is_fixed_pitch)
        pifi->jWinPitchAndFamily |= FIXED_PITCH;
    else
        pifi->jWinPitchAndFamily |= VARIABLE_PITCH;

    return TRUE;
}

PVOID
OtfFindTable(
    PVOID pvView,
    ULONG cjView,
    ULONG ulTag,
    PULONG pulLength)
{
    POTF_FILE_HEADER pFileHeader = pvView;
    ULONG i, ulOffset, ulLength, ulNumTables;

    /* Verify the file header */
    if (pFileHeader->ulIdentifier != 'OTTO' &&
        pFileHeader->ulIdentifier != 'fctt' &&
        pFileHeader->ulIdentifier != 0x00000100)
    {
        DbgPrint("ERROR: Couldn't verify identifier: 0x%lx\n",
                 pFileHeader->ulIdentifier);
        return NULL;
    }

    /* Check if number of tables is ok */
    ulNumTables = SWAPW(pFileHeader->usNumTables);
    ulLength = ulNumTables * sizeof(OTF_TABLE_ENTRY);
    if (ulLength + sizeof(OTF_FILE_HEADER) > cjView)
    {
        DbgPrint("ERROR: Too many tables (%ld)\n", ulNumTables);
        return NULL;
    }

    /* Loop all tables */
    for (i = 0; i < ulNumTables; i++)
    {
        /* Compare the tag */
        if (pFileHeader->aTableEntries[i].ulTag == ulTag)
        {
            /* Get table offset and length */
            ulOffset = SWAPD(pFileHeader->aTableEntries[i].ulOffset);
            ulLength = SWAPD(pFileHeader->aTableEntries[i].ulLength);

            /* Check if this is inside the file */
            if (ulOffset + ulLength > cjView ||
                ulOffset + ulLength < ulOffset)
            {
                DbgPrint("invalid table entry. %ld, %ld \n", ulOffset, ulLength);
                return NULL;
            }

            if (pulLength) *pulLength = ulLength;
            return (PUCHAR)pvView + ulOffset;
        }
    }

    /* Not found */
    return NULL;
}

BYTE
OtfGetWinFamily(BYTE jClassId, BYTE jSubclassId)
{
    BYTE jFamily = 0;

    switch (jClassId)
    {
        case 0: // Class ID = 0 No Classification
            /* We rely on the already set value */
            break;

        case 1: // Class ID = 1 Oldstyle Serifs
            switch (jSubclassId)
            {
                case 0: // Subclass ID = 0 : No Classification
                case 1: // Subclass ID = 1 : IBM Rounded Legibility
                case 2: // Subclass ID = 2 : Garalde
                case 3: // Subclass ID = 3 : Venetian
                case 4: // Subclass ID = 4 : Modified Venetian
                case 5: // Subclass ID = 5 : Dutch Modern
                case 6: // Subclass ID = 6 : Dutch Traditional
                case 7: // Subclass ID = 7 : Contemporary
                case 8: // Subclass ID = 8 : Calligraphic
                    jFamily = FF_SCRIPT; break;

                case 15: // Subclass ID = 15 : Miscellaneous

                default: // Subclass ID = 9-14 : (reserved for future use)
                    break;
            }
        case 2: // Class ID = 2 Transitional Serifs
        case 3: // Class ID = 3 Modern Serifs
        case 4: // Class ID = 4 Clarendon Serifs
        case 5: // Class ID = 5 Slab Serifs
        case 6: // Class ID = 6 (reserved for future use)
        case 7: // Class ID = 7 Freeform Serifs
        case 8: // Class ID = 8 Sans Serif
            switch (jSubclassId)
            {
                case 15: // Subclass ID = 15 : Miscellaneous
                    jFamily = FF_SWISS | FF_ROMAN; break;
                default:
                    break;
            }
        case 9: // Class ID = 9 Ornamentals
        case 10: // Class ID = 10 Scripts
        case 11: // Class ID = 11 (reserved for future use)
        case 12: // Class ID = 12 Symbolic
        case 13: // Class ID = 13 Reserved
        case 14: // Class ID = 14 Reserved
            break;
    }
__debugbreak();
    return jFamily;
}

VOID
NTAPI
OtfGetIfiMetrics(
    PFTFD_FACE pface,
    PIFIMETRICS pifi)
{
    PFTFD_FILE pfile = pface->pfile;
    PVOID pvView = pfile->pvView;
    POTF_OS2_DATA pOs2;

    /* Try to get type 1 info */
    OtfGetType1FontInfo(pface, pifi);

    /* Get the OS/2 table for the face */
    // FIXME: get the right table for the face, when multiple faces
    pOs2 = OtfFindTable(pvView, pfile->cjView, '2/SO', NULL);
    if (!pOs2)
    {
        DbgPrint("Couldn't find OS/2 table\n");
        return;
    }

    //pifi->lEmbedId;
    //pifi->lItalicAngle;
    //pifi->lCharBias;
    //pifi->jWinCharSet;
    pifi->jWinPitchAndFamily &= 3;
    pifi->jWinPitchAndFamily |= OtfGetWinFamily(pOs2->jClassId, pOs2->jSubClassId);
    pifi->usWinWeight = SWAPW(pOs2->usWeightClass);
    //pifi->flInfo;
    pifi->fsSelection = SWAPW(pOs2->fsSelection);
    pifi->fsType = SWAPW(pOs2->fsType);
    //pifi->fwdUnitsPerEm;
    //pifi->fwdLowestPPEm;
    pifi->fwdWinAscender = SWAPW(pOs2->usWinAscent);
    pifi->fwdWinDescender = SWAPW(pOs2->usWinDescent);
    //pifi->fwdMacAscender;
    //pifi->fwdMacDescender;
    //pifi->fwdMacLineGap;
    pifi->fwdTypoAscender = SWAPW(pOs2->sTypoAscender);
    pifi->fwdTypoDescender = SWAPW(pOs2->sTypoDescender);
    pifi->fwdTypoLineGap = SWAPW(pOs2->sTypoLineGap);
    pifi->fwdAveCharWidth = SWAPW(pOs2->xAvgCharWidth);
    //pifi->fwdMaxCharInc;
    pifi->fwdCapHeight = SWAPW(pOs2->sCapHeight);
    pifi->fwdXHeight = SWAPW(pOs2->sxHeight);
    pifi->fwdSubscriptXSize = SWAPW(pOs2->ySubscriptXSize);
    pifi->fwdSubscriptYSize = SWAPW(pOs2->ySubscriptYSize);
    pifi->fwdSubscriptXOffset = SWAPW(pOs2->ySubscriptXOffset);
    pifi->fwdSubscriptYOffset = SWAPW(pOs2->ySubscriptYOffset);
    pifi->fwdSuperscriptXSize = SWAPW(pOs2->ySuperscriptXSize);
    pifi->fwdSuperscriptYSize = SWAPW(pOs2->ySuperscriptYSize);
    pifi->fwdSuperscriptXOffset = SWAPW(pOs2->ySuperscriptXOffset);
    pifi->fwdSuperscriptYOffset = SWAPW(pOs2->ySuperscriptYOffset);
    //pifi->fwdUnderscoreSize;
    //pifi->fwdUnderscorePosition;
    pifi->fwdStrikeoutSize = SWAPW(pOs2->yStrikeoutSize);
    pifi->fwdStrikeoutPosition = SWAPW(pOs2->yStrikeoutPosition);
    pifi->wcFirstChar = SWAPW(pOs2->usFirstCharIndex);
    pifi->wcLastChar = SWAPW(pOs2->usLastCharIndex);
    pifi->wcDefaultChar = SWAPW(pOs2->usDefaultChar);
    pifi->wcBreakChar = SWAPW(pOs2->usBreakChar);
    //pifi->chFirstChar = (CHAR)pifi->wcFirstChar; // FIXME: convert
    //pifi->chLastChar = (CHAR)pifi->wcLastChar;
    pifi->chDefaultChar = (CHAR)pifi->wcDefaultChar;
    pifi->chBreakChar = (CHAR)pifi->wcBreakChar;
    //pifi->ptlBaseline;
    //pifi->ptlAspect;
    //pifi->ptlCaret;
    //pifi->rclFontBox;
    *(DWORD*)pifi->achVendId = *(DWORD*)pOs2->achVendID;
    //pifi->cKerningPairs;
    //pifi->ulPanoseCulture;
    pifi->panose = *(PANOSE*)pOs2->panose;
}

