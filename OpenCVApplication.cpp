#include "stdafx.h"
#include "common.h"
#include <windows.h>

using namespace std;
using namespace cv;

#define BITS 12
#define TABLE_SIZE 4096

struct LZWCodePack {
    unsigned short code1 : BITS;
    unsigned short code2 : BITS;
};

struct LZWCode {
    unsigned short codeValue : BITS;
};

struct LZWCodeComparator {
    bool operator()(const LZWCode& a, const LZWCode& b) const {
        return a.codeValue < b.codeValue;
    }
};

char filename[MAX_PATH];
string fileExtension = "";
Mat_<uchar> currentImage;
bool selectionCanceled;

void initSelectFileDialog(bool shouldSelectImage);
void writeLZWCodePackToFile(LZWCodePack lzw, FILE* f);
vector<LZWCode> readLZWCodesFromFile(FILE* f);
vector<LZWCode> encodeLZW(vector<uchar> pixels);
void encodeImage();
vector<uchar> decodeLZW(vector<LZWCode> codes);
void decodeLZWFile();

void initSelectFileDialog(bool shouldSelectImage) {
    fileExtension = "";

    OPENFILENAME ofn;
    ZeroMemory(&filename, sizeof(filename));
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;  
    ofn.lpstrFilter = shouldSelectImage ? "Grayscale BMP Image (*.bmp)\0*.bmp\0" : "Encoded Image (*.lzw)\0*.lzw\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = "Select a File";
    ofn.Flags = OFN_DONTADDTORECENT | OFN_FILEMUSTEXIST;

    if (GetOpenFileNameA(&ofn))
    {   
        selectionCanceled = false;
        std::cout << "You chose the file \"" << filename << "\"\n";
        if (shouldSelectImage) {
            currentImage = imread(filename, IMREAD_GRAYSCALE);
            cout << "Image size: " << currentImage.size << endl;
            fileExtension = "bmp";
        }
        else {
            fileExtension = "lzw";
        }
    }
    else
    {
        switch (CommDlgExtendedError())
        {
        case CDERR_DIALOGFAILURE: std::cout << "CDERR_DIALOGFAILURE\n";   break;
        case CDERR_FINDRESFAILURE: std::cout << "CDERR_FINDRESFAILURE\n";  break;
        case CDERR_INITIALIZATION: std::cout << "CDERR_INITIALIZATION\n";  break;
        case CDERR_LOADRESFAILURE: std::cout << "CDERR_LOADRESFAILURE\n";  break;
        case CDERR_LOADSTRFAILURE: std::cout << "CDERR_LOADSTRFAILURE\n";  break;
        case CDERR_LOCKRESFAILURE: std::cout << "CDERR_LOCKRESFAILURE\n";  break;
        case CDERR_MEMALLOCFAILURE: std::cout << "CDERR_MEMALLOCFAILURE\n"; break;
        case CDERR_MEMLOCKFAILURE: std::cout << "CDERR_MEMLOCKFAILURE\n";  break;
        case CDERR_NOHINSTANCE: std::cout << "CDERR_NOHINSTANCE\n";     break;
        case CDERR_NOHOOK: std::cout << "CDERR_NOHOOK\n";          break;
        case CDERR_NOTEMPLATE: std::cout << "CDERR_NOTEMPLATE\n";      break;
        case CDERR_STRUCTSIZE: std::cout << "CDERR_STRUCTSIZE\n";      break;
        case FNERR_BUFFERTOOSMALL: std::cout << "FNERR_BUFFERTOOSMALL\n";  break;
        case FNERR_INVALIDFILENAME: std::cout << "FNERR_INVALIDFILENAME\n"; break;
        case FNERR_SUBCLASSFAILURE: std::cout << "FNERR_SUBCLASSFAILURE\n"; break;
        default: std::cout << "Selection cancelled\n"; selectionCanceled = true;
        }
    }
}

void writeLZWCodePackToFile(LZWCodePack lzw, FILE* f) {
    unsigned short code1 = lzw.code1;
    unsigned short code2 = lzw.code2;
    uint32_t packed = code1 | ((uint32_t)code2 << 12);
    putc(packed, f);
    putc(packed >> 8, f);
    putc(packed >> 16, f);
}

vector<LZWCode> readLZWCodesFromFile(FILE* f) {
    vector<LZWCode> result;
    while (!feof(f)) {
        char byte1 = getc(f);
        char byte2 = getc(f);
        char byte3 = getc(f);
        unsigned short code1 = (((unsigned short)byte2 & 0xF) << 8) | byte1 & 0xFF;
        unsigned short code2 = (((unsigned short)byte3) & 0xFF) << 4 | ((unsigned short)byte2 & 0xF0) >> 4;
        result.push_back({ code1 });
        result.push_back({ code2 });
    }
    return result;
}

vector<LZWCode> encodeLZW(vector<uchar> pixels) {
    
    map<vector<uchar>, unsigned short> table;
    vector<uchar> current;
    vector<LZWCode> result;
    unsigned short code = 256;

    for (unsigned short i = 0; i < 256; i++) {
        table[{(uchar)i}] = i;
    }

    current = { pixels.at(0) };

    for (auto i = 1; i < pixels.size(); i++) {
        uchar pixel = pixels.at(i);

        vector<uchar> newList = vector<uchar>(current);
        newList.emplace_back(pixel);

        if (table.find(newList) == table.end()) {
            LZWCode newCode = { table[current] };
            result.emplace_back(newCode);
            
            if (TABLE_SIZE > code) {
                table[newList] = code++;
            }
            current = { pixel };
        }
        else {
            current.emplace_back(pixel);
        }
    }

    LZWCode lastCode = { table[current] };
    result.emplace_back(lastCode);

    return result;
}

void encodeImage() {

    FILE* f;
    vector<LZWCode> encoded;
    vector<uchar> pixels;
    string encodedFilename;
    float compressedDataSize;
    float compressionRatio;
    int nrOfCodes;
    int imageSize;
    
    encodedFilename = string(filename);
    encodedFilename = encodedFilename.substr(0, encodedFilename.length() - 4);
    encodedFilename += "_encoded.lzw";

    for (int i = 0; i < currentImage.rows; i++)
        for (int j = 0; j < currentImage.cols; j++) {
            pixels.emplace_back(currentImage(i, j));
        }

    encoded = encodeLZW(pixels);
    nrOfCodes = encoded.size();

    f = fopen(encodedFilename.c_str(), "wb");
    fwrite(&nrOfCodes, sizeof(int), 1, f);
    fwrite(&currentImage.cols, sizeof(int), 1, f);

    for (auto it = encoded.begin(); it != encoded.end(); it += 2) {
        if (it + 1 != encoded.end()) {
            LZWCodePack pack = { (*it).codeValue, (*(it + 1)).codeValue };
            writeLZWCodePackToFile(pack, f);
        }
        else {
            LZWCodePack pack = { (*(it)).codeValue, 0 };
            writeLZWCodePackToFile(pack, f);
            break;
        }
    }

    compressedDataSize = ftell(f);
    fclose(f);

    imageSize = currentImage.rows * currentImage.cols;
    compressionRatio = imageSize / compressedDataSize;
    
    cout << "LZW Encoding finished..." << endl;
    cout << "Saved encoded image: " + encodedFilename << endl;
    cout << "Image size(bytes): " << imageSize << endl;
    cout << "Compressed data size(bytes): " << compressedDataSize << endl;
    cout << "Compression ratio: " << compressionRatio << endl;
}

vector<uchar> decodeLZW(vector<LZWCode> codes) {

    map<LZWCode, vector<uchar>, LZWCodeComparator> table;
    LZWCode oldCode;
    vector<uchar> result;
    unsigned short code = 256;

    for (unsigned short i = 0; i < 256; i++) {
        table[{i}] = { (uchar)i };
    }

    oldCode = codes.at(0);
    result.emplace_back(table[oldCode].at(0));
    
    for (auto i = 1; i < codes.size(); i++) {
        LZWCode nextCode = codes.at(i);
        vector<uchar> translation;

        if (table.find(nextCode) == table.end()) {
            translation = table[oldCode];
            translation.emplace_back(translation.at(0));
        }
        else {
            translation = table[nextCode];
        }

        result.insert(result.end(), translation.begin(), translation.end());

        if (TABLE_SIZE > code) {
            vector<uchar> newEntry = table[oldCode];
            newEntry.emplace_back(translation.at(0));
            LZWCode newKey = { code++ };
            table[newKey] = newEntry;
        }
        oldCode = nextCode;
    }

    return result;
}

void decodeLZWFile() {

    FILE* f;
    int nrOfCodes, resultImageRows, resultImageCols;
    vector<uchar> pixels;
    Mat_<uchar> result;
    string resultFilename;

    f = fopen(filename, "rb");
    fread(&nrOfCodes, sizeof(int), 1, f);
    fread(&resultImageCols, sizeof(int), 1, f);
    vector<LZWCode> codes = readLZWCodesFromFile(f);
    fclose(f);

    for (auto i = 0; i < codes.size() - nrOfCodes; i++) {
        codes.pop_back();
    }

    pixels = decodeLZW(codes);

    resultImageRows = pixels.size() / resultImageCols;

    result = Mat_<uchar>(resultImageRows, resultImageCols, &pixels[0]);

    resultFilename = string(filename);
    resultFilename = resultFilename.substr(0, resultFilename.length() - 12);
    resultFilename += "_decoded.bmp";

    cout << "LZW Decoding finished..." << endl;
    cout << "Saved decoded image: " + resultFilename << endl;
    cout << "Image size(bytes): " << resultImageRows * resultImageCols << endl;
    
    imwrite(resultFilename, result);
    imshow("Decoded Image", result);
    waitKey(0);
}

int main() {
    bool shouldQuit = false;
    int action = 0;

    while (!shouldQuit) {

        cout << "####################" << endl;
        cout << "# Actions:         #" << endl;
        cout << "#  1.Encode image  #" << endl;
        cout << "#  2.Decode image  #" << endl;
        cout << "#  3.Exit          #" << endl;
        cout << "####################" << endl;
        cout << "  Select action: ";
        cin >> action;
        cout << endl;
        switch (action)
        {
        case 1:
            initSelectFileDialog(true);
            if (selectionCanceled) break;
            encodeImage();
            break;
        case 2:
            initSelectFileDialog(false);
            if (selectionCanceled) break;
            decodeLZWFile();
            break;
        case 3:
            shouldQuit = true;
            break;
        default:
            cout << "Invalid command" << endl;
        }

        cout <<"\n";
    }

	return 0;
}
