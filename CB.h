#ifndef __CB_H__
#define __CB_H__

namespace CB {
struct Position {
  char board[64];
  struct CastlingInfo {
    bool WK = false;
    bool WQ = false;
    bool BK = false;
    bool BQ = false;
  } canCastle;

  enum class Commentary { Good, Bad, Indifferent } comment;

  int epFile;
  unsigned int totalGames;
  unsigned int whiteWins;
  unsigned int blackWins;
  unsigned int draws;
};
}
#endif  // __CB_H__
