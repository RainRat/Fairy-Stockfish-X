<h2 align="center">ffish.js</h2>

<p align="center">
<a href="https://img.shields.io/badge/-ffish.js-green"><img src="https://img.shields.io/badge/-ffish.js-green" alt="Package"></a>
<a href="https://npmcharts.com/compare/ffish?minimal=true"><img src="https://img.shields.io/npm/dm/ffish.svg?sanitize=true" alt="Downloads"></a>
<a href="https://www.npmjs.com/package/ffish"><img src="https://img.shields.io/npm/v/ffish.svg?sanitize=true" alt="Version"></a>
</p>

<p align="center">
<a href="https://img.shields.io/badge/-ffish--es6.js-green"><img src="https://img.shields.io/badge/-ffish--es6.js-green" alt="Package-ES6"></a>
<a href="https://npmcharts.com/compare/ffish-es6?minimal=true"><img src="https://img.shields.io/npm/dm/ffish-es6.svg?sanitize=true" alt="Downloads-ES6"></a>
<a href="https://www.npmjs.com/package/ffish-es6"><img src="https://img.shields.io/npm/v/ffish-es6.svg?sanitize=true" alt="Version-ES6"></a>
</p>


The **ffish.js** package is a WebAssembly chess variant library based on [_Fairy-Stockfish_](https://github.com/ianfab/Fairy-Stockfish).

It is available as a [standard module](https://www.npmjs.com/package/ffish) and an [ES6 module](https://www.npmjs.com/package/ffish-es6), with an API modeled after [python-chess](https://python-chess.readthedocs.io/en/latest/index.html).

## Install instructions

### Standard module

```bash
npm install ffish
```

### ES6 module
```bash
npm install ffish-es6
```

## Examples

Load the API in JavaScript:

### Standard module

```javascript
const ffish = require('ffish');
```

### ES6 module

```javascript
import Module from 'ffish-es6';
let ffish = null;

new Module().then(loadedModule => {
    ffish = loadedModule;
    console.log(`initialized ${ffish} ${loadedModule}`);
});
```

### Available variants

Show all available variants supported by _Fairy-Stockfish_ and **ffish.js**.

```javascript
ffish.variants()
```
```
3check 3check-crazyhouse 5check ai-wok almost amazon anti-losalamos antichess\
armageddon asean ataxx atomic breakthrough bughouse cambodian capablanca\
capahouse caparandom centaur cfour chancellor chaturanga chess chessgi chigorin\
clobber clobber10 codrus coffeehouse courier crazyhouse dobutsu embassy euroshogi\
extinction fairy fischerandom flipello flipersi gardner gemini giveaway gorogoro\
gothic grand grandhouse hoppelpoppel horde indiangreat janggi janggicasual\
janggihouse janggimodern janggitraditional janus jesonmor judkins karouk kinglet\
kingofthehill knightmate koedem kyotoshogi loop losalamos losers makpong makruk\
makrukhouse manchu micro mini minishogi minixiangqi modern newzealand nocastle\
nocheckatomic normal orda pawnsonly peasant placement pocketknight racingkings\
seirawan semitorpedo shako shatar shatranj shogi shogun shouse sittuyin suicide\
supply threekings tictactoe upsidedown weak xiangqi xiangqihouse
```

### Board object

Create a new variant board from its default starting position.
The event `onRuntimeInitialized` ensures that the wasm file was properly loaded.

```javascript
ffish['onRuntimeInitialized'] = () => {
  let board = new ffish.Board("chess");
}
```

Set a custom FEN position with validation:
```javascript
fen = "rnb1kbnr/ppp1pppp/8/3q4/8/8/PPPP1PPP/RNBQKBNR w KQkq - 0 3";
if (ffish.validateFen(fen) == 1) {  // returns 1 for FEN_OK
    board.setFen(fen);
}
else {
    console.error(`Invalid FEN string.`);
}
```

Alternatively, you can initialize a board with a custom FEN directly:
```javascript
let board2 = new ffish.Board("chess", "rnb1kbnr/ppp1pppp/8/3q4/8/8/PPPP1PPP/RNBQKBNR w KQkq - 0 3");
```

### ASCII board

Print a simple text board using `toString()`:

```javascript
let board = new ffish.Board("chess", "rnb1kbnr/ppp1pppp/8/3q4/8/8/PPPP1PPP/RNBQKBNR w KQkq - 0 3");
console.log(board.toString())
```
```
r n b . k b n r
p p p . p p p p
. . . . . . . .
. . . q . . . .
. . . . . . . .
. . . . . . . .
P P P P . P P P
R N B Q K B N R
```

or a detailed board with coordinates using `.toVerboseString()`:

```javascript
let board = new ffish.Board("chess", "rnb1kbnr/ppp1pppp/8/3q4/8/8/PPPP1PPP/RNBQKBNR w KQkq - 0 3");
console.log(board.toVerboseString())
```
```
+---+---+---+---+---+---+---+---+
| r | n | b |   | k | b | n | r |8
+---+---+---+---+---+---+---+---+
| p | p | p |   | p | p | p | p |7
+---+---+---+---+---+---+---+---+
|   |   |   |   |   |   |   |   |6
+---+---+---+---+---+---+---+---+
|   |   |   | q |   |   |   |   |5
+---+---+---+---+---+---+---+---+
|   |   |   |   |   |   |   |   |4
+---+---+---+---+---+---+---+---+
|   |   |   |   |   |   |   |   |3
+---+---+---+---+---+---+---+---+
| P | P | P | P |   | P | P | P |2
+---+---+---+---+---+---+---+---+
| R | N | B | Q | K | B | N | R |1 *
+---+---+---+---+---+---+---+---+
  a   b   c   d   e   f   g   h

Fen: rnb1kbnr/ppp1pppp/8/3q4/8/8/PPPP1PPP/RNBQKBNR w KQkq - 0 3
Sfen: rnb1kbnr/ppp1pppp/8/3q4/8/8/PPPP1PPP/RNBQKBNR b - 5
Key: 39B6F80E84D75BFB
Checkers:
```

## Move generation and application

Add a new move:
```javascript
board.push("g2g4");
```

Special variant moves use suffix characters in the coordinate string:

- clone moves: `e2e4c`
- swap moves: `d4e4s`
- self-destruct moves: `c3c4x`
- pull moves: `e5f5,d5`

Generate all legal moves in UCI and SAN notation:
```javascript
let legalMoves = board.legalMoves().split(" ");
let legalMovesSan = board.legalMovesSan().split(" ");

for (var i = 0; i < legalMovesSan.length; i++) {
    console.log(`${i}: ${legalMoves[i]}, ${legalMovesSan[i]}`)
}
```

## Memory management

Emscripten does not automatically invoke destructors on C++ objects. Call `.delete()` to free heap memory when finished with an object.
```javascript
board.delete();
```

## PGN parsing

Parse a PGN game string from a file. Call `game.delete()` and `board.delete()` when done to free WebAssembly memory.

```javascript
fs = require('fs');
let pgnFilePath = "data/pgn/kasparov-deep-blue-1997.pgn"

fs.readFile(pgnFilePath, 'utf8', function (err,data) {
  if (err) {
    return console.log(err);
  }
  game = ffish.readGamePGN(data);
  console.log(game.headerKeys());
  console.log(game.headers("White"));
  const mainlineMoves = game.mainlineMoves().split(" ");

  let board = new ffish.Board(game.headers("Variant").toLowerCase());
  for (let idx = 0; idx < mainlineMoves.length; ++idx) {
      board.push(mainlineMoves[idx]);
  }
  // or use board.pushMoves(game.mainlineMoves()); to push all moves at once

  let finalFen = board.fen();
  board.delete();
  game.delete();
}
```

## Custom variants

Fairy-Stockfish also allows defining custom variants by loading a configuration file.

See e.g. the configuration for **connect4**, **tictactoe** or **janggihouse** in [variants.ini](https://github.com/ianfab/Fairy-Stockfish/blob/master/src/variants.ini).
```javascript
fs = require('fs');
let configFilePath = './variants.ini';
 fs.readFile(configFilePath, 'utf8', function (err,data) {
   if (err) {
     return console.log(err);
   }
   ffish.loadVariantConfig(data)
   let board = new ffish.Board("tictactoe");
   board.delete();
 });
```

## Additional features

For examples of each available function, see [test.js](https://github.com/ianfab/Fairy-Stockfish/blob/master/tests/js/test.js).

## Build instructions

Built with Emscripten/Embind from C++ source code.

* https://emscripten.org/docs/porting/connecting_cpp_and_javascript/embind.html


To disable variants with boards larger than 8x8, add the flag `largeboards=no`.

The pre-compiled wasm binary is built with `largeboards=yes`.

Set `debug=yes` when running tests.


### Compile as standard module

```bash
cd src
make -f Makefile_js build
```

### Compile as ES6/ES2015 module

Environments such as [Vue.js](https://vuejs.org/) or modern bundlers require the ES6 module build:

```bash
cd src
make -f Makefile_js build es6=yes
```

Ensure the wasm file is in your `public` directory.

Reference: [emscripten/#10114](https://github.com/emscripten-core/emscripten/issues/10114)

### Compile in docker
Instead of installing emscripten natively you can also run the compilation in docker from this directory using e.g.

```bash
DOCKER_USER=$(id -u):$(id -g) docker compose run --rm emscripten make -f Makefile_js build es6=yes
```

## Instructions to run the tests
```bash
npm install
npm test
```

## Instructions to run the example server
```bash
npm install
```
```bash
node index.js
```

## Example Projects

### ffish-test

A simple toy website which demonstrates the core functionality of ffish.js and [chessgroundx](https://github.com/gbtami/chessgroundx).

Source code: https://github.com/thearst3rd/ffish-test

See it deployed at: https://thearst3rd.github.io/ffish-test/
