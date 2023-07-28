#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <fstream>
#include <queue>
#include <cmath>
#include <unordered_map>

using namespace std;

struct PageTable {
    string type = "UNMAPPED";
    int frameNumber;
    int onDisk = 0;
    vector<bool> pageIndex;
};

struct FrameTable {
    int inUse = 0;
    int dirty = 0;
    int firstUse = 1;
    int lastUse;
};

struct InputSizes {
    int pageSize;
    int numFrames;
    int numPages;
    int numBackingBlocks;
};

struct ReferenceTracker {
    int pagesReferencedCount = 0;
    int pagesMappedCount = 0;
    int pageMissesCount = 0;
    int framesTakenCount = 0;
    int framesWrittenToDiskCount = 0;
    int framesRecoveredFromDisk = 0;
};

int debug = 0;

vector<PageTable> buildPageTable(int numPages, int pageSize) {
    vector<PageTable> pageTable(numPages);
    vector<bool> taken(pageSize, false);
    for (size_t i = 0; i < pageTable.size(); i++) {
        pageTable[i].pageIndex = taken;
    }
    return pageTable;
}

vector<FrameTable> buildFrameTable(int numFrames) {
    vector<FrameTable> frameTable(numFrames);
    return frameTable;
}

void printPageTable(vector<PageTable> pageTable) {
    cout << "Page Table" << endl;
    for (size_t i = 0; i < pageTable.size(); i++) {
        if (i < 10) {
            cout << "    " ;
        } else {
            cout << "   ";
        }
        cout << i;
        if (pageTable[i].type == "UNMAPPED") {
            cout << " type:" << pageTable[i].type << endl;
        } else {
            cout << " type:" << pageTable[i].type << " framenum:" << pageTable[i].frameNumber << " ondisk:" << pageTable[i].onDisk << endl;
        }
    }
}

void printFrameTable(vector<FrameTable> frameTable) {
    cout << "Frame Table" << endl;
    for (size_t i = 0; i < frameTable.size(); i++) {
        if (frameTable[i].inUse) {
            cout << "    " << i << " inuse:" << frameTable[i].inUse << " dirty:" << frameTable[i].dirty << " first_use:" << frameTable[i].firstUse << " last_use:" << frameTable[i].lastUse << endl;
        } else {
            cout << "    " << i << " inuse:" << frameTable[i].inUse << endl;
        }
    }
}

void printReferenceTracker(ReferenceTracker tracker) {
    cout << "Pages referenced: " << tracker.pagesReferencedCount << endl;
    cout << "Pages mapped: " << tracker.pagesMappedCount << endl;
    cout << "Page misses: " << tracker.pageMissesCount << endl;
    cout << "Frames taken: " << tracker.framesTakenCount << endl;
    cout << "Frames written to disk: " << tracker.framesWrittenToDiskCount << endl;
    cout << "Frames recovered from disk: " << tracker.framesRecoveredFromDisk << endl;
}

void printResults(ReferenceTracker tracker, vector<PageTable> pageTable, vector<FrameTable> frameTable) {
    printPageTable(pageTable);
    printFrameTable(frameTable);
    printReferenceTracker(tracker);
}

ifstream getPageAndFrameValues(string file, InputSizes &inputSizes) {
    ifstream inputFile;
    string comments;
    inputFile.open(file);
    if (inputFile.fail()) {
        cout << "Opening input file failed" << endl;
        exit(-1);
    }
    while (inputFile.peek() == '#' && getline(inputFile, comments));
    inputFile >> inputSizes.pageSize;
    inputFile >> inputSizes.numFrames;
    inputFile >> inputSizes.numPages;
    inputFile >> inputSizes.numBackingBlocks;
    return inputFile;
}

void takeFrameIfNecessary(vector<PageTable> &pageTable, vector<FrameTable> &frameTable, ReferenceTracker &tracker, int frame) {
    for (size_t i = 0; i < pageTable.size(); i++) {
        if (pageTable[i].type == "MAPPED" && pageTable[i].frameNumber == frame) {
            if (debug) {
                cout << "Frame taken from page: " << i << endl;
            }
            tracker.framesTakenCount++;
            pageTable[i].type = "TAKEN";
            if (frameTable[pageTable[i].frameNumber].dirty) {
                pageTable[i].onDisk = 1;
                tracker.framesWrittenToDiskCount++;
            }
            pageTable[i].frameNumber = -1;            
        }
    }
}

void updatePageAndFrameTable (vector<PageTable> &pageTable, vector<FrameTable> &frameTable, ReferenceTracker &tracker, int frame, int requestedPage,  int requestedIndexInPage, int instructionCount) {
    if (pageTable[requestedPage].onDisk) {
        tracker.framesRecoveredFromDisk++;
    }
    if (pageTable[requestedPage].type == "UNMAPPED") {
        tracker.pagesMappedCount++;
    }
    tracker.pageMissesCount++;
    pageTable[requestedPage].type = "MAPPED";
    pageTable[requestedPage].frameNumber = frame;
    pageTable[requestedPage].pageIndex[requestedIndexInPage] = true;
    frameTable[frame].inUse = 1;
    frameTable[frame].firstUse = instructionCount;
    frameTable[frame].lastUse = instructionCount;
  
}

void read(vector<PageTable> &pageTable, vector<FrameTable> &frameTable, ReferenceTracker &tracker, int frame, int requestedPage,  int requestedIndexInPage, int instructionCount) {
    takeFrameIfNecessary(pageTable, frameTable, tracker, frame);
    updatePageAndFrameTable(pageTable, frameTable, tracker, frame, requestedPage, requestedIndexInPage, instructionCount);
    frameTable[frame].dirty = 0;
}

void write(vector<PageTable> &pageTable, vector<FrameTable> &frameTable, ReferenceTracker &tracker, int frame, int requestedPage,  int requestedIndexInPage, int instructionCount) {
    takeFrameIfNecessary(pageTable, frameTable, tracker, frame);
    updatePageAndFrameTable(pageTable, frameTable, tracker, frame, requestedPage, requestedIndexInPage, instructionCount);
    frameTable[frame].dirty = 1;
}

void printInitialSizes(InputSizes inputSizes, string reclaimAlgorithm) {
    cout << "Page size: "<< inputSizes.pageSize << endl;
    cout << "Num frames: " << inputSizes.numFrames << endl;
    cout << "Num pages: " << inputSizes.numPages << endl;
    cout << "Num backing blocks: " << inputSizes.numBackingBlocks << endl;
    cout << "Reclaim algorithm: " << reclaimAlgorithm << endl;
}


int findLeastRecentlyUsedFrame(unordered_map<int, int> frameLastUsed) {
    int leastRecentlyUsedFrame = 0;
    int minValue = INT32_MAX;
    for (const auto& item : frameLastUsed) {
        if (item.second < minValue) {
            minValue = item.second;
            leastRecentlyUsedFrame = item.first;
        }
    }
    return leastRecentlyUsedFrame;
}

void doOptimal() {
    cout << "I don't implement optimal" << endl;
    exit(0);
}

void readOrWrite(vector<PageTable> &pageTable, vector<FrameTable> &frameTable, ReferenceTracker &tracker, int nextUsableFrame, int requestedPage, int requestedIndexInPage, int instructionCount, string instruction) {
    if (instruction == "w") {
        write(pageTable, frameTable, tracker, nextUsableFrame, requestedPage, requestedIndexInPage, instructionCount);
    } else {
        read(pageTable, frameTable, tracker, nextUsableFrame, requestedPage, requestedIndexInPage, instructionCount);
    }
}

void processInput(string reclaimAlgorithm, string file) {
    InputSizes inputSizes;
    ReferenceTracker tracker;
    ifstream inputFile = getPageAndFrameValues(file, inputSizes);
    printInitialSizes(inputSizes, reclaimAlgorithm);
    vector<PageTable> pageTable = buildPageTable(inputSizes.numPages, inputSizes.pageSize);
    vector<FrameTable> frameTable = buildFrameTable(inputSizes.numFrames);
    string currentInstruction;
    string instruction;
    int requestedPage;
    int requestedIndexInPage;
    int instructionCount = 1;
    inputFile.ignore();
    queue<int> framesQueue;
    for (size_t i = 0; i < frameTable.size(); i++) {
        framesQueue.push(i);
    }
    unordered_map<int, int> leastRecentUsedFrame;
    for (int i = frameTable.size() - 1; i >= 0; i--) {
        leastRecentUsedFrame.insert({i, 0});
    }
    while (getline(inputFile, currentInstruction)) {
        if (currentInstruction[0] == '#') {
            continue;
        }
        if (debug) {
            cout << "Current Instructions: " << currentInstruction << endl;
        }
        stringstream ss(currentInstruction);
        ss >> instruction;
        if (instruction == "print") {
            printResults(tracker, pageTable, frameTable);
        } else if (instruction == "debug") {
            debug = 1;
        } else if (instruction == "nodebug") {
            debug = 0;
        } else {
            ss >> requestedPage;
            requestedIndexInPage = requestedPage % inputSizes.pageSize;
            requestedPage = floor((double)requestedPage / (double)inputSizes.pageSize);
            if (debug) {
                cout << "Requested Page: " << requestedPage << " Requested Page Index: " << requestedIndexInPage << endl;
            }
            if (pageTable[requestedPage].type == "MAPPED") {
                if (instruction == "w") {
                    frameTable[pageTable[requestedIndexInPage].frameNumber].dirty = 1;
                }
                pageTable[requestedPage].pageIndex[requestedIndexInPage] = true;
                frameTable[pageTable[requestedPage].frameNumber].lastUse = instructionCount;
                leastRecentUsedFrame[pageTable[requestedPage].frameNumber] = instructionCount;
            } else {
                if (reclaimAlgorithm == "LRU") {
                    int nextUsableFrame = findLeastRecentlyUsedFrame(leastRecentUsedFrame);
                    if (debug) {
                        cout << "Next Usable Frame LRU: " << nextUsableFrame << endl;
                    }
                    readOrWrite(pageTable, frameTable, tracker, nextUsableFrame, requestedPage, requestedIndexInPage, instructionCount, instruction);
                    leastRecentUsedFrame[nextUsableFrame] = instructionCount;
                } else if (reclaimAlgorithm == "FIFO") {
                    int nextUsableFrame = framesQueue.front();
                    if (debug) {
                        cout << "Next Usable Frame FIFO: " << nextUsableFrame << endl;
                    }
                    readOrWrite(pageTable, frameTable, tracker, nextUsableFrame, requestedPage, requestedIndexInPage, instructionCount, instruction);
                    framesQueue.pop();
                    framesQueue.push(nextUsableFrame);
                } else {
                    doOptimal();
                }
            }
            tracker.pagesReferencedCount++;
            instructionCount++;
        }
    }
    printResults(tracker, pageTable, frameTable);
}

int main(int argc, char* argv[]) {
    string reclaimAlgorithm;
    string file;
    if (argc == 3) {
        reclaimAlgorithm = argv[1];
        file = argv[2];
    } else if (argc == 4) {
        reclaimAlgorithm = argv[2];
        file = argv[3];
    } else {
        exit(-1);
    }
    processInput(reclaimAlgorithm, file);
    return 0;
}