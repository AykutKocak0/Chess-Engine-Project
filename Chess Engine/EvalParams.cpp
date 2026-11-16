#include "EvalParams.h"
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>

// The engine's active evaluation weights. The default values live in the
// member initializers in EvalParams.h.
EvalParams evalParams;

namespace {

enum class Kind { Int, Pair, IntArray, PairArray, Table };

struct Field {
    const char* decl;   // C++ declaration, e.g. "int passedPawnMg[8]"
    const char* name;   // name used in the text format
    size_t offset;
    int count;          // number of ints
    Kind kind;
    int rowLen;         // values per line when printing arrays
};

#define F_INT(m)         { "int " #m, #m, offsetof(EvalParams, m), 1, Kind::Int, 1 }
#define F_PAIR(m)        { "EvalPair " #m, #m, offsetof(EvalParams, m), 2, Kind::Pair, 1 }
#define F_INTS(m, n, r)  { "int " #m "[" #n "]", #m, offsetof(EvalParams, m), n, Kind::IntArray, r }
#define F_PAIRS(m, n)    { "EvalPair " #m "[" #n "]", #m, offsetof(EvalParams, m), 2 * n, Kind::PairArray, 1 }
#define F_TABLE(m)       { "int " #m "[6][64]", #m, offsetof(EvalParams, m), 6 * 64, Kind::Table, 8 }

// Every member of EvalParams, in declaration order. Add new members here too;
// paramFieldsComplete() catches a forgotten one.
const Field FIELDS[] = {
    F_INT(tempo),
    F_PAIRS(material, 5),
    F_PAIR(bishopPair),
    F_PAIR(centerPawn),
    F_PAIR(centerPawnDuo),
    F_PAIR(doubledPawn),
    F_PAIR(isolatedPawn),
    F_PAIR(protectedPassedPawn),
    F_INTS(passedPawnMg, 8, 8),
    F_INTS(passedPawnEg, 8, 8),
    F_TABLE(pstMg),
    F_TABLE(pstEg),
    F_INTS(knightMobilityMg, 9, 9),
    F_INTS(knightMobilityEg, 9, 9),
    F_INTS(bishopMobilityMg, 14, 14),
    F_INTS(bishopMobilityEg, 14, 14),
    F_INTS(rookMobilityMg, 15, 15),
    F_INTS(rookMobilityEg, 15, 15),
    F_PAIR(queenNoMoves),
    F_PAIR(queenFewMoves),
    F_INT(kingAttackKnight),
    F_INT(kingAttackBishop),
    F_INT(kingAttackRook),
    F_INT(kingAttackQueen),
    F_INTS(kingSafetyTable, 100, 10),
    F_INT(kingFileNoOwnPawn),
    F_INT(kingFileFullyOpen),
    F_INT(kingAdjFilesNoOwnPawn),
    F_INT(kingCenterOpenFile),
    F_INT(shieldPawnUnmoved),
    F_INT(shieldPawnAdvanced),
    F_INT(shieldPawnMissing),
    F_INT(trappedBishop7th),
    F_INT(trappedBishop6th),
    F_INT(knightInCorner),
    F_INT(trappedKnight7th),
    F_PAIR(pinnedPiece),
    F_PAIR(pawnThreatQueen),
    F_PAIR(pawnThreatRook),
    F_PAIR(pawnThreatMinor),
    F_PAIR(minorThreatQueen),
    F_PAIR(minorThreatRook),
};

const int* fieldData(const EvalParams& p, const Field& f) {
    return reinterpret_cast<const int*>(reinterpret_cast<const char*>(&p) + f.offset);
}
int* fieldData(EvalParams& p, const Field& f) {
    return reinterpret_cast<int*>(reinterpret_cast<char*>(&p) + f.offset);
}

}  // namespace

bool paramFieldsComplete() {
    size_t expected = 0;
    for (const Field& f : FIELDS) {
        if (f.offset != expected) {
            std::cerr << "EvalParams field list: gap or wrong order before " << f.name << "\n";
            return false;
        }
        expected += f.count * sizeof(int);
    }
    if (expected != sizeof(EvalParams)) {
        std::cerr << "EvalParams field list: members missing at the end\n";
        return false;
    }
    return true;
}

std::string paramName(int index) {
    int start = 0;
    for (const Field& f : FIELDS) {
        if (index < start + f.count) {
            int i = index - start;
            std::ostringstream s;
            s << f.name;
            switch (f.kind) {
                case Kind::Int:       break;
                case Kind::Pair:      s << (i == 0 ? ".mg" : ".eg"); break;
                case Kind::IntArray:  s << "[" << i << "]"; break;
                case Kind::PairArray: s << "[" << i / 2 << "]" << (i % 2 == 0 ? ".mg" : ".eg"); break;
                case Kind::Table:     s << "[" << i / 64 << "][" << i % 64 << "]"; break;
            }
            return s.str();
        }
        start += f.count;
    }
    return "?";
}

bool saveParams(const std::string& path, const EvalParams& p) {
    std::ofstream out(path);
    if (!out) return false;
    for (const Field& f : FIELDS) {
        const int* v = fieldData(p, f);
        out << f.name;
        for (int i = 0; i < f.count; i++) out << " " << v[i];
        out << "\n";
    }
    return bool(out);
}

bool loadParams(const std::string& path, EvalParams& p) {
    std::ifstream in(path);
    if (!in) {
        std::cerr << "cannot open " << path << "\n";
        return false;
    }
    std::string line;
    int lineNo = 0;
    while (std::getline(in, line)) {
        lineNo++;
        std::istringstream s(line);
        std::string name;
        if (!(s >> name) || name[0] == '#') continue;
        const Field* field = nullptr;
        for (const Field& f : FIELDS) {
            if (name == f.name) field = &f;
        }
        if (!field) {
            std::cerr << path << ":" << lineNo << ": unknown parameter " << name << "\n";
            return false;
        }
        int values[6 * 64];
        for (int i = 0; i < field->count; i++) {
            if (!(s >> values[i])) {
                std::cerr << path << ":" << lineNo << ": " << name << " needs "
                          << field->count << " values\n";
                return false;
            }
        }
        int* v = fieldData(p, *field);
        for (int i = 0; i < field->count; i++) v[i] = values[i];
    }
    return true;
}

bool saveParamsCpp(const std::string& path, const EvalParams& p) {
    std::ofstream out(path);
    if (!out) return false;
    static const char* pieceNames[6] = { "pawn", "knight", "bishop", "rook", "queen", "king" };
    for (const Field& f : FIELDS) {
        const int* v = fieldData(p, f);
        out << "    " << f.decl << " = ";
        switch (f.kind) {
            case Kind::Int:
                out << v[0] << ";\n";
                break;
            case Kind::Pair:
                out << "{" << v[0] << ", " << v[1] << "};\n";
                break;
            case Kind::PairArray:
                out << "{ ";
                for (int i = 0; i < f.count; i += 2) {
                    out << (i ? ", " : "") << "{" << v[i] << ", " << v[i + 1] << "}";
                }
                out << " };\n";
                break;
            case Kind::IntArray:
                if (f.count <= f.rowLen) {
                    out << "{ ";
                    for (int i = 0; i < f.count; i++) out << (i ? ", " : "") << v[i];
                    out << " };\n";
                } else {
                    out << "{\n";
                    for (int i = 0; i < f.count; i++) {
                        if (i % f.rowLen == 0) out << "        ";
                        out << v[i] << ",";
                        out << (i % f.rowLen == f.rowLen - 1 ? "\n" : " ");
                    }
                    out << "    };\n";
                }
                break;
            case Kind::Table:
                out << "{\n";
                for (int piece = 0; piece < 6; piece++) {
                    out << "        { // " << pieceNames[piece] << "\n";
                    for (int sq = 0; sq < 64; sq++) {
                        if (sq % 8 == 0) out << "            ";
                        char buf[16];
                        snprintf(buf, sizeof(buf), "%4d,", v[piece * 64 + sq]);
                        out << buf << (sq % 8 == 7 ? "\n" : " ");
                    }
                    out << "        },\n";
                }
                out << "    };\n";
                break;
        }
    }
    return bool(out);
}
