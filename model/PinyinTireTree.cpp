//
// Created by 唐艺峰 on 17/3/10.
//


#include "PinyinTireTree.h"

PinyinTireTree::PinyinTireTree() {
    config = std::ifstream("../config.is");
    dic = fopen("../library.is", "rb+");
}

// Methods for tree

void PinyinTireTree::initial() {
    std::map<std::string, std::string> pinyinConfig;
    root = new PinyinNode;
    char line[32];
    std::string singleConfig;
    while (config.getline(line, sizeof(line))) {
        std::stringstream singleLine(line);
        std::string info[3];
        singleLine >> info[0];
        singleLine >> info[1];
        singleLine >> info[2];
        pinyinConfig[info[0]] = info[1] + " " + info[2];
    }
    for (std::map<std::string, std::string>::iterator it = pinyinConfig.begin();
         it != pinyinConfig.end(); it++) {
        addNode(root, it->first, it->second);
    }
}

void PinyinTireTree::addNode(PinyinNode *node, std::string key, std::string data) {
    if (key.length() == 0) {
        std::stringstream singleLine(data);
        std::string info[2];
        singleLine >> info[0];
        singleLine >> info[1];
        node->addressStart = std::stol(info[0]);
        node->length = (short) std::stoi(info[1]);
        node->hasChild = true;
    } else {
        char cur = key.at(0);
        if (node->children[cur - 'a'] == NULL) {
            node->children[cur - 'a'] = new PinyinNode;
        }
        addNode(node->children[cur - 'a'], key.substr(1, key.length() - 1), data);
    }
}

// Methods for character

std::map<std::vector<char>, double, ValueComparator> *PinyinTireTree::getCharacters(std::string key) {
    curChars = new std::vector<std::vector<char>>;
    curValue = new std::vector<double>;
    curAddress = new std::vector<int>;
    curWords = new std::vector<std::string>;
    curWordsValue = new std::vector<double>;
    curLengthOfWords = new std::vector<int>;
    isClear = true;
    return getChars(root, key);
}

std::map<std::vector<char>, double, ValueComparator> *PinyinTireTree::getChars(PinyinNode *node, std::string key) {
    if (key.length() == 0) {
        if (node->hasChild) {
            if (curNode == NULL)
                curNode = node;
            fseek(dic, node->addressStart, 0);
            std::map<std::vector<char>, double, ValueComparator> *output =
                    new std::map<std::vector<char>, double, ValueComparator>(ValueComparator(curChars, curValue));
            for (int i = 0; i < node->length; i++) {
                char singChar[3];
                fread(singChar, 1, 3, dic);
                std::vector<char> *c = new std::vector<char>;
                double d;
                int a;
                int l;
                c->insert(c->end(), singChar[0]);
                c->insert(c->end(), singChar[1]);
                c->insert(c->end(), singChar[2]);
                fread(&d, 8, 1, dic);
                fread(&a, 4, 1, dic);
                fread(&l, 4, 1, dic);
                curChars->insert(curChars->end(), *c);
                curValue->insert(curValue->end(), d);
                curAddress->insert(curAddress->end(), a);
                curLengthOfWords->insert(curLengthOfWords->end(), l);
                (*output)[*c] = d;
            }
            if (!isClear) {
                for (int i = 0; i < 26; i++) {
                    PinyinNode *child = node->children[i];
                    if (child != NULL) {
                        std::map<std::vector<char>, double, ValueComparator> *candidate = getChars(child, key);
                        if (candidate != NULL) {
                            output->insert(candidate->begin(), candidate->end());
                        }
                    }
                }
            }
            return output;
        } else {
            std::map<std::vector<char>, double, ValueComparator> *output =
                    new std::map<std::vector<char>, double, ValueComparator>(ValueComparator(curChars, curValue));
            isClear = false;
            for (int i = 0; i < 26; i++) {
                PinyinNode *child = node->children[i];
                if (child != NULL) {
                    std::map<std::vector<char>, double, ValueComparator> *candidate = getChars(child, key);
                    if (candidate != NULL) {
                        output->insert(candidate->begin(), candidate->end());
                    }
                }
            }
            return output;
        }
    } else {
        char cur = key.at(0);
        return getChars(node->children[cur - 'a'], key.substr(1, key.length() - 1));
    }
}

std::map<std::string, double, WordsValueComparator> *PinyinTireTree::chooseCharacter(std::vector<char> ch) {
    if (ch.at(0) != NOT_CHOOSE) {
        std::vector<std::vector<char>>::iterator it = std::find(curChars->begin(), curChars->end(), ch);
        int pos = (int) (it - curChars->begin());
        double x = curValue->at((unsigned long) pos);
        double arctanh = (0.5 * (log((1. + x) / (1. - x))) / log(E)) * 1000.;
        double v;

        fseek(dic, curNode->addressStart, 0);
        for (int i = 0; i < curChars->size(); i++) {
            if (i == pos) {
                v = tanh((arctanh + VALUE_INCREASE_STEP) / 1000.);
            } else {
                v = curValue->at((unsigned long) i) * VALUE_DECAY_RATE;
            }
            fseek(dic, 3, 1);
            fwrite(&v, 8, 1, dic);
            fflush(dic);
            fseek(dic, 8, 1);
        }
        return getWords(pos);
    }
    curNode = NULL;
    return NULL;
}

// Methods for words

std::map<std::string, double, WordsValueComparator> *PinyinTireTree::getWords(int pos) {
    int length = curLengthOfWords->at((unsigned long) pos);
    wordStartAddress = curAddress->at((unsigned long) pos);
    if (wordStartAddress != NO_WORDS) {
        FILE *wordDic = fopen("../WordsDic.txt", "rb+");
        std::map<std::string, double, WordsValueComparator> *wordMap =
                new std::map<std::string, double, WordsValueComparator>(WordsValueComparator(curWords, curWordsValue));
        fseek(wordDic, wordStartAddress, 0);
        for (int i = 0; i < length; i++) {
            unsigned short len;
            unsigned char lw, lh;
            std::string *word = new std::string;
            char cWord[64];
            double value;
            fread(&lw, 1, 1, wordDic);
            fread(&lh, 1, 1, wordDic);
            len = (lw << 8) | lh;
            fread(cWord, 1, len, wordDic);
            fread(&value, 8, 1, wordDic);
            for (int j = 0; j < len; j++) {
                word->insert(word->end(), cWord[j]);
            }
            curWords->insert(curWords->end(), *word);
            curWordsValue->insert(curWordsValue->end(), value);
            (*wordMap)[*word] = value;
        }
        fclose(wordDic);
        return wordMap;
    }
    return new std::map<std::string, double, WordsValueComparator>(WordsValueComparator(curWords, curWordsValue));
}

void PinyinTireTree::chooseWord(std::string word) {
    if (word != NOT_CHOOSE_S) {
        std::vector<std::string>::iterator it = std::find(curWords->begin(), curWords->end(), word);
        int pos = (int) (it - curWords->begin());
        double x = curValue->at((unsigned long) pos);
        double arctanh = (0.5 * (log((1. + x) / (1. - x))) / log(E)) * 1000.;
        double v;

        FILE *wordDic = fopen("../WordsDic.txt", "rb+");
        fseek(wordDic, wordStartAddress, 0);
        for (int i = 0; i < curWords->size(); i++) {
            if (i == pos) {
                v = tanh((arctanh + VALUE_INCREASE_STEP) / 1000.);
            } else {
                v = curWordsValue->at((unsigned long) i) * VALUE_DECAY_RATE;
            }
            unsigned short len;
            unsigned char lw, lh;
            fread(&lw, 1, 1, wordDic);
            fread(&lh, 1, 1, wordDic);
            len = (lw << 8) | lh;
            fread(&len, 2, 1, wordDic);
            fseek(wordDic, len, 1);
            fwrite(&v, 8, 1, wordDic);
            fflush(wordDic);
        }
        fclose(wordDic);
    }
}

ValueComparator::ValueComparator(std::vector<std::vector<char>> *_curChars, std::vector<double> *_curValue) {
    curChars = _curChars;
    curValue = _curValue;
}

WordsValueComparator::WordsValueComparator(std::vector<std::string> *_curWords, std::vector<double> *_curWordsValue) {
    curWords = _curWords;
    curWordsValue = _curWordsValue;
}

bool ValueComparator::operator()(const std::vector<char> &a, const std::vector<char> &b) const {
    std::vector<std::vector<char>>::iterator it1 = std::find(curChars->begin(), curChars->end(), a);
    std::vector<std::vector<char>>::iterator it2 = std::find(curChars->begin(), curChars->end(), b);
    int pos1 = (int) (it1 - curChars->begin());
    int pos2 = (int) (it2 - curChars->begin());
    return (curValue->at((unsigned long) pos1) >= curValue->at((unsigned long) pos2));
};

bool WordsValueComparator::operator()(const std::string &a, const std::string &b) const {
    std::vector<std::string>::iterator it1 = std::find(curWords->begin(), curWords->end(), a);
    std::vector<std::string>::iterator it2 = std::find(curWords->begin(), curWords->end(), b);
    int pos1 = (int) (it1 - curWords->begin());
    int pos2 = (int) (it2 - curWords->begin());
    return curWordsValue->at((unsigned long) pos1) >= curWordsValue->at((unsigned long) pos2);
};