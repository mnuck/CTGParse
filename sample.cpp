#include <endian.h>
#include <stdint.h>

#include <iostream>
#include <fstream>
#include <vector>

#include "CB.h"

/* http://rybkaforum.net/cgi-bin/rybkaforum/topic_show.pl?tid=2319
   read this. */

std::vector<CB::Position> parsePage(char *buffer) {
  std::vector<CB::Position> result;
  int byteCursor = 0;
  int bitCursor = 0;

  auto positionCount = be16toh(*((uint16_t *)(buffer + byteCursor)));
  byteCursor += 2;

  auto validBytes = be16toh(*((uint16_t *)(buffer + byteCursor)));
  byteCursor += 2;

  auto readBit = [&]() -> bool {
    --bitCursor;
    if (-1 == bitCursor) {
      bitCursor = 7;
      ++byteCursor;
    }
    return *(buffer + byteCursor) & (1 << bitCursor);
  };

  for (int positionIndex = 0; positionIndex < positionCount; positionIndex++) {
    if (byteCursor > validBytes) break;
    CB::Position cb;

    uint8_t positionHeader = *((uint8_t *)(buffer + byteCursor));
    // uint8_t positionLength = positionHeader & 0x1f;
    bool containsEnPassant = positionHeader & 0x20;
    bool containsCastling = positionHeader & 0x40;

    int squaresEmitted = 0;
    auto emit = [&](char c) { cb.board[squaresEmitted] = c; };
    while (squaresEmitted < 64) {
      if (readBit()) {    // there is a piece here
        if (readBit()) {  // P
          if (readBit())
            emit('p');
          else
            emit('P');
        } else {              // RBNQK
          if (readBit()) {    // RB
            if (readBit()) {  // R
              if (readBit())
                emit('r');
              else
                emit('R');
            } else {  // B
              if (readBit())
                emit('b');
              else
                emit('B');
            }
          } else {            // NQK
            if (readBit()) {  // N
              if (readBit())
                emit('n');
              else
                emit('N');
            } else {            // QK
              if (readBit()) {  // Q
                if (readBit())
                  emit('q');
                else
                  emit('Q');
              } else {  // K
                if (readBit())
                  emit('k');
                else
                  emit('K');
              }
            }
          }
        }
      } else {  // no piece here
        emit('.');
      }

      ++squaresEmitted;
    }

    int target = 0;
    if (containsEnPassant)
      if (containsCastling)
        target = 7;
      else
        target = 3;
    else if (containsCastling)
      target = 4;
    else
      target = 0;

    while (bitCursor != target) readBit();

    if (containsEnPassant) {
      if (readBit()) cb.epFile |= 0x04;
      if (readBit()) cb.epFile |= 0x02;
      if (readBit()) cb.epFile |= 0x01;
    } else {
      cb.epFile = -1;
    }

    if (containsCastling) {
      if (readBit()) cb.canCastle.WK = true;
      if (readBit()) cb.canCastle.WQ = true;
      if (readBit()) cb.canCastle.BK = true;
      if (readBit()) cb.canCastle.BQ = true;
    }

    byteCursor += 1;  // done parsing board, moving on

    uint8_t bytesForMoves = *((uint8_t *)(buffer + byteCursor));

    byteCursor += bytesForMoves;
    --byteCursor;  // offset to read 3 bytes into 4 and mask

    uint32_t gamesSeen = *((uint32_t *)(buffer + byteCursor));
    gamesSeen = be32toh(gamesSeen);
    gamesSeen &= 0x00FFFFFF;
    cb.totalGames = gamesSeen;

    byteCursor += 3;
    uint32_t whiteWins = *((uint32_t *)(buffer + byteCursor));
    whiteWins = be32toh(whiteWins);
    whiteWins &= 0x00FFFFFF;
    cb.whiteWins = whiteWins;

    byteCursor += 3;
    uint32_t blackWins = *((uint32_t *)(buffer + byteCursor));
    blackWins = be32toh(blackWins);
    blackWins &= 0x00FFFFFF;
    cb.blackWins = blackWins;

    byteCursor += 3;
    uint32_t draws = *((uint32_t *)(buffer + byteCursor));
    draws = be32toh(draws);
    draws &= 0x00FFFFFF;
    cb.draws = draws;

    ++byteCursor;  // fix offset from the read-3-into-4 sequence

    byteCursor += 4;
    // unknown integer

    byteCursor += 16;
    // ranking stuff

    byteCursor += 1;
    // uint8_t recommendation = *((uint8_t *)(buffer + byteCursor));

    byteCursor += 1;
    // unknown byte (weight somehow?)

    byteCursor += 1;
    uint8_t commentary = *((uint8_t *)(buffer + byteCursor));
    cb.comment = CB::Position::Commentary::Indifferent;
    if (commentary & 0x80) cb.comment = CB::Position::Commentary::Good;
    if (commentary & 0x40) cb.comment = CB::Position::Commentary::Bad;

    byteCursor += 1;
    // advance to next position entry

    result.push_back(cb);
  }

  return result;
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cout << "need a filename for your CTG file" << std::endl;
    return 1;
  }

  using std::ios;

  std::ifstream infile;
  infile.open(argv[1], ios::binary | ios::in);
  infile.seekg(0, ios::end);

  auto fileSize = infile.tellg();
  const size_t PAGESIZE = 4096;

  int pageCount = fileSize / PAGESIZE;

  char initialGame[] =
      "RP....prNP....pnBP....pbQP....pqKP....pkBP....pbNP....pnRP....pr";

  for (int i = 1; i < pageCount; ++i) {
    infile.seekg(4096 * i, ios::beg);
    char buffer[PAGESIZE];
    infile.read(buffer, PAGESIZE);
    auto boards = parsePage(buffer);
    for (auto &b : boards) {
      bool found = true;
      for (int i = 0; i < 64; ++i)
        if (b.board[i] != initialGame[i]) {
          found = false;
          break;
        }
      if (found) {
        for (int i = 0; i < 64; ++i) {
          int y = i / 8;
          int x = i % 8;
          if (x == 0) std::cout << std::endl;
          std::cout << b.board[(7 - y) + (x * 8)] << " ";
        }
        std::cout << std::endl;
        std::cout << "total games: " << b.totalGames << std::endl;
        std::cout << "white wins: " << b.whiteWins << std::endl;
      }
    }
  }
  infile.close();

  return 0;
}
