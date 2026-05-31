"use strict";

const express = require('express')
require('./fetch-shim.js').setupFetchShim();

const ffish = require('./ffish.js');
const { PerformanceObserver, performance } = require('perf_hooks');
const { Chess } = require('chess.js')
const { Crazyhouse } = require('crazyhouse.js')

const app = express();

app.get('/', (req, res) => {

  const board = new ffish.Board("chess");
  let legalMoves = board.legalMoves();

  let it = 1000;

  console.log("Standard Chess")
  console.log("==================")

  var t0 = performance.now()
  for (let i = 0; i < it; ++i) {
     legalMoves = board.legalMoves().split(" ");
  }
  var t1 = performance.now()
  console.log(`Call to board.legalMoves()+legalMoves.split(" ") took ${(t1 - t0).toFixed(2)}  milliseconds.`)

  t0 = performance.now()
  for (let i = 0; i < it; ++i) {
    legalMoves = board.legalMovesSan().split(" ")
  }
  t1 = performance.now()
  console.log(`board.legalMovesSan().split(" ").length: ${legalMoves.length}`)
  console.log(`Call to board.legalMovesSan()+legalMoves.split(" ") took ${(t1 - t0).toFixed(2)}  milliseconds.`)

  board.delete();

  // pass in a FEN string to load a particular position
  const chess = new Chess(
      "rnb1kbnr/ppp1pppp/8/3q4/8/8/PPPP1PPP/RNBQKBNR w KQkq - 0 3"
  )
  t0 = performance.now()
  for (let i = 0; i < it; ++i) {
    legalMoves = chess.moves()
  }
  t1 = performance.now()
  console.log(`chess.moves().length: ${legalMoves.length}`)
  console.log(`Call to chess.moves() took ${(t1 - t0).toFixed(2)}  milliseconds.`)

  console.log("Crazyhouse")
  console.log("===========")

  let crazyhouseFen = "rnb1kb1r/ppp2ppp/4pn2/8/3P4/2N2Q2/PPP2PPP/R1B1KB1R/QPnp b KQkq - 0 6";
  const board2 = new ffish.Board("crazyhouse", crazyhouseFen);

  t0 = performance.now()
  for (let i = 0; i < it; ++i) {
    legalMoves = board2.legalMovesSan().split(" ")
  }
  t1 = performance.now()
  console.log(`board.legalMoves().split(" ").length: ${legalMoves.length}`)
  console.log(`Call to board.legalMoves() took ${(t1 - t0).toFixed(2)}  milliseconds.`)

  const czMoves = ["e4", "d5", "exd5", "Qxd5", "Nf3", "Nf6", "Nc3", "e6", "d4", "Qxf3", "Qxf3"]
  // pass in a FEN string to load a particular position
  const crazyhouse = new Crazyhouse()

  for (let idx = 0; idx < czMoves.length; ++idx) {
    crazyhouse.move(czMoves[idx])
  }

  t0 = performance.now()
  for (let i = 0; i < it; ++i) {
    legalMoves = crazyhouse.moves()
  }
  t1 = performance.now()
  console.log(`crazyhouse.moves().length: ${legalMoves.length}`)
  console.log(`Call to crazyhouse.moves() took ${(t1 - t0).toFixed(2)}  milliseconds.`)

  let legalMovesSan = board2.legalMovesSan().split(" ");

  for (var idx = 0; idx < legalMovesSan.length; idx++) {
      console.log(`${idx}: ${legalMoves[idx]}, ${legalMovesSan[idx]}`);
  }
  console.log(board2.fen());

  board2.delete();

  res.send(String("Test server of ffish.js"));
});

app.listen(8000, "127.0.0.1", () => {
  console.log('Test server of ffish.js listening on port 8000.')
  console.log('http://127.0.0.1:8000/')
});
