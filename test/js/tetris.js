$(document).ready(function() {
    var exports = {};
    "use strict";
    Object.defineProperty(exports, "__esModule", { value: true });
    var GAME_MODE;
    (function (GAME_MODE) {
        GAME_MODE[GAME_MODE["TITLE"] = 1] = "TITLE";
        GAME_MODE[GAME_MODE["PLAYING"] = 2] = "PLAYING";
        GAME_MODE[GAME_MODE["PAUSED"] = 3] = "PAUSED";
    })(GAME_MODE || (GAME_MODE = {}));
    var GamePadState = /** @class */ (function () {
        function GamePadState(buttonNum, keyName) {
            this.buttonDown = false;
            this.buttonNum = -1;
            this.buttonTimer = 0;
            this.keyName = '';
            this.buttonNum = buttonNum;
            this.keyName = keyName;
        }
        return GamePadState;
    }());
    exports.GamePadState = GamePadState;
    var MyApp = /** @class */ (function () {
        function MyApp() {
            this.message = '';
            // keytimer = 0;
            this.downkeytimer = 0;
            this.leftkeytimer = 0;
            this.rightkeytimer = 0;
            this.game_mode = GAME_MODE.TITLE;
            this.waitForDownKeyRelease = false;
            this.gamepadButtons = [];
            /* KEYBOARD CONTROLS */
            this.dropKey = false;
            this.upKey = false;
            /* TOUCH CONTROLS */
            this.touchDirection = '';
            this.touchX_Start = 0;
            this.touchX_InitialStart = 0;
            this.touchY_Start = 0;
            this.touch_threshold = 25;
            this.lastTouch = new Date();
            this.touchTimer = 0;
            this.touchMoved = false;
            //slowly move the piece down
            //rather than fast drop
            this.touchSlowDownCounter = 0;
            this.touchSlowDownMode = false;
            //draw:boolean = false;
            this.lastCalledTime = new Date();
            this.fpscounter = 0;
            this.currentfps = 0;
            //this.bindRivets();
            this.createGameTable();
            this.initGame();
            // smurf;
            requestAnimationFrame(this.requestNextFrame);
            document.onkeydown = this.keyDown;
            document.onkeyup = this.keyUp;
            //window.addEventListener("gamepadconnected", this.initGamePad.bind(this));
            //window.addEventListener("resize", this.onWindowResize.bind(this));
        }
        /* GAMEPAD CONTROLS */

        MyApp.prototype.sendKeyDownEvent = function (key) {
            var keyEvent = new KeyboardEvent('Gamepad Event Down', { key: key });
            this.keyDown(keyEvent);
        };
        MyApp.prototype.sendKeyUpEvent = function (key) {
            var keyEvent = new KeyboardEvent('Gamepad Event Up', { key: key });
            this.keyUp(keyEvent);
        };
        MyApp.prototype.keyDown = function (event) {
            var app = window.myApp;
            //alert(event.key);
            if (event.key == 'ArrowDown' || event.key == 'Down')
                app.downKey = true;
            if (event.key == 'ArrowLeft' || event.key == 'Left') {
                app.leftKey = true;
            }
            if (event.key == 'ArrowRight' || event.key == 'Right') {
                app.rightKey = true;
            }
            if (event.key == 'a' && !app.dropKey) {
                app.drop();
                app.dropKey = true;
            }
            if ((event.key == 'ArrowUp' || event.key == 'Up') && !app.upKey) {
                app.rotate();
                app.upKey = true;
            }
            if (event.key == 'p') {
                app.btnPause();
            }
            if (event.key == 'n') {
                app.newGame_Click();
            }
        };
        MyApp.prototype.keyUp = function (event) {
            var app = window.myApp;
            // console.log(event);
            if (event.key == 'a') {
                app.dropKey = false;
            }
            if ((event.key == 'ArrowUp' || event.key == 'Up')) {
                app.upKey = false;
            }
            if (event.key == 'ArrowDown' || event.key == 'Down') {
                app.waitForDownKeyRelease = false;
                app.downKey = false;
            }
            if (event.key == 'ArrowLeft' || event.key == 'Left') {
                //app.leftKey = false;
            }
            if (event.key == 'ArrowRight' || event.key == 'Right') {
                //app.rightKey = false;
            }
        };
       MyApp.prototype.findGreatest = function (nums) {
            var greatest = 0;
            var greatestIndex = 0;
            for (var i = 0; i < nums.length; i++) {
                if (nums[i] > greatest) {
                    greatestIndex = i;
                    greatest = nums[i];
                }
            }
            return greatestIndex;
        };
        MyApp.prototype.getRandomNumber = function (max) {
            return Math.floor(Math.random() * Math.floor(max));
        };
        MyApp.prototype.newGame_Click = function () {
            this.reset();
        };
        MyApp.prototype.initGame = function () {
            this.fps = 0;
            this.nextLevel = 0;
            this.timer = 0;
            this.piece = 0;
            this.state = 0;
            this.centX = 0;
            this.centY = 0;
            this.score = 0;
            this.level = 0;
            this.lines = 0;
            this.nextPiece = this.getRandomNumber(7) + 1;
            this.toClear = false;
            this.toMakePiece = true;
            this.downKey = false;
            this.leftKey = false;
            this.rightKey = false;
            this.gameMatrix = [];
            this.gameMatrixBuffer = [];
            this.delay = 32;
            for (var i = 0; i < 20; i++) {
                var arr1 = [];
                var arr2 = [];
                this.gameMatrix.push(arr1);
                this.gameMatrixBuffer.push(arr2);
                for (var j = 0; j < 10; j++) {
                    this.gameMatrix[i][j] = 0;
                    this.gameMatrixBuffer[i][j] = 0;
                }
            }
            this.nextPieceMatrix = [];
            for (var i = 0; i < 4; i++) {
                var arr3 = [];
                this.nextPieceMatrix.push(arr3);
                for (var j = 0; j < 5; j++)
                    this.nextPieceMatrix[i][j] = 0;
            }
            this.gamepadButtons.push(new GamePadState(14, 'Left'));
            this.gamepadButtons.push(new GamePadState(15, 'Right'));
            this.gamepadButtons.push(new GamePadState(13, 'Down'));
            this.gamepadButtons.push(new GamePadState(0, 'Up'));
            this.gamepadButtons.push(new GamePadState(1, 'Up'));
            this.gamepadButtons.push(new GamePadState(12, 'a'));
            this.gamepadButtons.push(new GamePadState(9, 'p'));
            this.gamepadButtons.push(new GamePadState(8, 'n'));
        };
        MyApp.prototype.reset = function () {
            this.game_mode = GAME_MODE.PLAYING;
            for (var i = 0; i < 4; i++) {
                for (var j = 0; j < 5; j++)
                    this.nextPieceMatrix[i][j] = 0;
            }
            this.nextLevel = 0;
            this.timer = 0;
            this.piece = 0;
            this.state = 0;
            this.centX = 0;
            this.centY = 0;
            this.score = 0;
            this.level = 1;
            this.lines = 0;
            this.waitForDownKeyRelease = false;
            this.nextPiece = this.getRandomNumber(7) + 1;
            this.toClear = false;
            this.toMakePiece = true;
            this.downKey = false;
            this.leftKey = false;
            this.rightKey = false;
            for (var i = 0; i < 20; i++) {
                for (var j = 0; j < 10; j++) {
                    this.gameMatrix[i][j] = 0;
                    this.gameMatrixBuffer[i][j] = 0;
                }
            }
        };
        MyApp.prototype.gameLoop = function () {
            //program loop
            if (this.game_mode != GAME_MODE.PLAYING) {
                return;
            }
            //put delays on directional keys - for right/left move once then wait
            if (this.downKey)
                this.downkeytimer++;
            else
                this.downkeytimer = 0;
            if (this.leftKey)
                this.leftkeytimer++;
            else
                this.leftkeytimer = 0;
            if (this.rightKey)
                this.rightkeytimer++;
            else
                this.rightkeytimer = 0;
            if (this.level == 1)
                this.levelSpeed = 30;
            if (this.level == 2)
                this.levelSpeed = 25;
            if (this.level == 3)
                this.levelSpeed = 20;
            if (this.level == 4)
                this.levelSpeed = 17;
            if (this.level == 5)
                this.levelSpeed = 14;
            if (this.level == 6)
                this.levelSpeed = 10;
            if (this.level == 7)
                this.levelSpeed = 8;
            if (this.level == 8)
                this.levelSpeed = 5;
            if (this.level == 9)
                this.levelSpeed = 3;
            if (this.level == 10)
                this.levelSpeed = 2;
            if (this.timer == -1)
                this.checkMatrix();
            if (this.toClear)
                this.timer--;
            else
                this.timer++;
            if (this.timer == -24) {
                this.clearBlocks();
                this.toClear = false;
                this.timer = 0;
            }
            if (this.toMakePiece && this.timer >= 0) {
                this.makePiece();
                this.toMakePiece = false;
                if (this.downKey || this.touchSlowDownMode) //prevent constant downkey if new piece is generated
                 {
                    this.waitForDownKeyRelease = true;
                }
            }
            if (this.downKey && !this.toClear && !this.waitForDownKeyRelease) {
                if (this.downkeytimer % 3 == 0)
                    this.moveDown();
            }
            if (this.leftKey && !this.toClear) {
                if (this.leftkeytimer == 1 || (this.leftkeytimer > 15 && this.leftkeytimer % 3 == 0)) {
                    this.moveLeft();
                    this.leftKey = 0;
				}
            }
            if (this.rightKey && !this.toClear) {
                if (this.rightkeytimer == 1 || (this.rightkeytimer > 15 && this.rightkeytimer % 3 == 0)) {
                    this.moveRight();
                    this.rightKey = 0;
                }
            }
            if (this.timer == this.levelSpeed) {
                this.timer = 0;
                this.moveDown();
            }
            this.findShadow();
        };
        MyApp.prototype.gameover = function () {
            //var referrer = document.referrer;
            //if (referrer == null || referrer == "")
            //    referrer = "NONE";
            //if (document.location.href.toLocaleLowerCase().indexOf('neilb.net') > 1)
            //    $.get('https://neilb.net/tetrisjsbackend/api/stuff/addscore?level=' + this.level + '&lines=' + this.lines + '&referrer=' + referrer);
            this.game_mode = GAME_MODE.TITLE;
            for (var i = 0; i < 4; i++)
                for (var j = 0; j < 5; j++)
                    this.nextPieceMatrix[i][j] = 0;
            for (var i = 0; i < 20; i++) {
                for (var j = 0; j < 10; j++) {
                    this.gameMatrix[i][j] = 0;
                    this.gameMatrixBuffer[i][j] = 0;
                }
            }
        };
        MyApp.prototype.makePiece = function () {
            this.piece = this.nextPiece;
            this.nextPiece = this.getRandomNumber(7) + 1;
            for (var i = 0; i < 4; i++)
                for (var j = 0; j < 5; j++)
                    this.nextPieceMatrix[i][j] = 0;
            if (this.gameMatrix[1][4] != 0) {
                this.gameover();
                return;
            }
            if (this.nextPiece == 1) {
                this.nextPieceMatrix[1][2] = 1;
                this.nextPieceMatrix[2][2] = 1;
                this.nextPieceMatrix[2][1] = 1;
                this.nextPieceMatrix[2][3] = 1;
            }
            if (this.nextPiece == 2) {
                this.nextPieceMatrix[1][3] = 2;
                this.nextPieceMatrix[1][2] = 2;
                this.nextPieceMatrix[2][2] = 2;
                this.nextPieceMatrix[2][1] = 2;
            }
            if (this.nextPiece == 3) {
                this.nextPieceMatrix[1][1] = 3;
                this.nextPieceMatrix[1][2] = 3;
                this.nextPieceMatrix[2][2] = 3;
                this.nextPieceMatrix[2][3] = 3;
            }
            if (this.nextPiece == 4) {
                this.nextPieceMatrix[1][3] = 4;
                this.nextPieceMatrix[2][3] = 4;
                this.nextPieceMatrix[2][2] = 4;
                this.nextPieceMatrix[2][1] = 4;
            }
            if (this.nextPiece == 5) {
                this.nextPieceMatrix[1][1] = 5;
                this.nextPieceMatrix[2][1] = 5;
                this.nextPieceMatrix[2][2] = 5;
                this.nextPieceMatrix[2][3] = 5;
            }
            if (this.nextPiece == 6) {
                this.nextPieceMatrix[1][3] = 6;
                this.nextPieceMatrix[1][2] = 6;
                this.nextPieceMatrix[2][2] = 6;
                this.nextPieceMatrix[2][3] = 6;
            }
            if (this.nextPiece == 7) {
                this.nextPieceMatrix[0][2] = 7;
                this.nextPieceMatrix[1][2] = 7;
                this.nextPieceMatrix[2][2] = 7;
                this.nextPieceMatrix[3][2] = 7;
            }
            if (this.piece == 1) {
                this.gameMatrixBuffer[0][5] = 1;
                this.gameMatrixBuffer[1][5] = 1;
                this.gameMatrixBuffer[1][4] = 1;
                this.gameMatrixBuffer[1][6] = 1;
                this.centX = 5;
                this.centY = 1;
                this.state = 1;
            }
            if (this.piece == 2) {
                this.gameMatrixBuffer[0][5] = 2;
                this.gameMatrixBuffer[0][4] = 2;
                this.gameMatrixBuffer[1][4] = 2;
                this.gameMatrixBuffer[1][3] = 2;
                this.centX = 4;
                this.centY = 1;
                this.state = 1;
            }
            if (this.piece == 3) {
                this.gameMatrixBuffer[0][3] = 3;
                this.gameMatrixBuffer[0][4] = 3;
                this.gameMatrixBuffer[1][4] = 3;
                this.gameMatrixBuffer[1][5] = 3;
                this.centX = 4;
                this.centY = 1;
                this.state = 1;
            }
            if (this.piece == 4) {
                this.gameMatrixBuffer[0][5] = 4;
                this.gameMatrixBuffer[1][5] = 4;
                this.gameMatrixBuffer[1][4] = 4;
                this.gameMatrixBuffer[1][3] = 4;
                this.centX = 4;
                this.centY = 1;
                this.state = 1;
            }
            if (this.piece == 5) {
                this.gameMatrixBuffer[0][3] = 5;
                this.gameMatrixBuffer[1][3] = 5;
                this.gameMatrixBuffer[1][4] = 5;
                this.gameMatrixBuffer[1][5] = 5;
                this.centX = 4;
                this.centY = 1;
                this.state = 1;
            }
            if (this.piece == 6) {
                this.gameMatrixBuffer[0][5] = 6;
                this.gameMatrixBuffer[0][4] = 6;
                this.gameMatrixBuffer[1][4] = 6;
                this.gameMatrixBuffer[1][5] = 6;
            }
            if (this.piece == 7) {
                this.gameMatrixBuffer[0][5] = 7;
                this.gameMatrixBuffer[1][5] = 7;
                this.gameMatrixBuffer[2][5] = 7;
                this.gameMatrixBuffer[3][5] = 7;
                this.centX = 5;
                this.centY = 1;
                this.state = 1;
            }
        };
        MyApp.prototype.moveLeft = function () {
            if (this.game_mode != GAME_MODE.PLAYING)
                return;
            var success = true;
            for (var i = 0; i < 10; i++) {
                for (var j = 0; j < 20; j++) {
                    if (this.gameMatrixBuffer[j][i] != 0 && i == 0)
                        success = false;
                    if (this.gameMatrixBuffer[j][i] != 0 && i != 0 && this.gameMatrix[j][i - 1] != 0)
                        success = false;
                }
            }
            if (success) {
                this.centX -= 1;
                for (var i = 1; i < 10; i++) {
                    for (var j = 0; j < 20; j++) {
                        if (this.gameMatrixBuffer[j][i] != 0) {
                            this.gameMatrixBuffer[j][i - 1] = this.gameMatrixBuffer[j][i];
                            this.gameMatrixBuffer[j][i] = 0;
                        }
                    }
                }
            }
        };
        MyApp.prototype.moveRight = function () {
            if (this.game_mode != GAME_MODE.PLAYING)
                return;
            var success = true;
            for (var i = 9; i > -1; i--) {
                for (var j = 0; j < 20; j++) {
                    if (this.gameMatrixBuffer[j][i] != 0 && i == 9)
                        success = false;
                    if (this.gameMatrixBuffer[j][i] != 0 && i != 9 && this.gameMatrix[j][i + 1] != 0)
                        success = false;
                }
            }
            if (success) {
                this.centX += 1;
                for (var i = 8; i > -1; i--) {
                    for (var j = 0; j < 20; j++) {
                        if (this.gameMatrixBuffer[j][i] != 0) {
                            this.gameMatrixBuffer[j][i + 1] = this.gameMatrixBuffer[j][i];
                            this.gameMatrixBuffer[j][i] = 0;
                        }
                    }
                }
            }
        };
        MyApp.prototype.drop = function () {
            var counter = 0; //failsafe
            while (this.toMakePiece == false) {
                counter++;
                if (counter > 100)
                    return;
                if (this.game_mode == GAME_MODE.PLAYING)
                    this.moveDown();
            }
        };
        MyApp.prototype.findShadow = function () {
            //initialize shadowFinderMatrix
            if (!this.shadowFinderMatrix) {
                this.shadowFinderMatrix = [];
                for (var i = 0; i < 20; i++) {
                    var arr1 = [];
                    this.shadowFinderMatrix.push(arr1);
                    for (var j = 0; j < 10; j++) {
                        this.shadowFinderMatrix[i][j] = 0;
                    }
                }
            }
            //copy gameMatrixBuffer to shadowFinderMatrix
            for (var i = 0; i < 20; i++) {
                for (var j = 0; j < 10; j++) {
                    this.shadowFinderMatrix[i][j] = this.gameMatrixBuffer[i][j];
                }
            }
            var shadowFound = false;
            var preventInfiniteLoopCounter = 0;
            while (shadowFound == false) {
                preventInfiniteLoopCounter++;
                if (preventInfiniteLoopCounter > 20)
                    break;
                for (var i = 0; i < 10; i++) {
                    for (var j = 0; j < 20; j++) {
                        if (this.shadowFinderMatrix[j][i] != 0 && j == 19)
                            shadowFound = true;
                        if (this.shadowFinderMatrix[j][i] != 0 && j != 19 && this.gameMatrix[j + 1][i] != 0)
                            shadowFound = true;
                    }
                }
                if (!shadowFound) {
                    for (var i = 19; i > -1; i--) {
                        for (var j = 0; j < 10; j++) {
                            if (this.shadowFinderMatrix[i][j] != 0) {
                                this.shadowFinderMatrix[i + 1][j] = this.shadowFinderMatrix[i][j];
                                this.shadowFinderMatrix[i][j] = 0;
                            }
                        }
                    }
                }
            }
        };
        MyApp.prototype.moveDown = function () {
            var success = true;
            for (var i = 0; i < 10; i++) {
                for (var j = 0; j < 20; j++) {
                    if (this.gameMatrixBuffer[j][i] != 0 && j == 19)
                        success = false;
                    if (this.gameMatrixBuffer[j][i] != 0 && j != 19 && this.gameMatrix[j + 1][i] != 0)
                        success = false;
                }
            }
            if (success) {
                this.centY += 1;
                for (var i = 19; i > -1; i--) {
                    for (var j = 0; j < 10; j++) {
                        if (this.gameMatrixBuffer[i][j] != 0) {
                            this.gameMatrixBuffer[i + 1][j] = this.gameMatrixBuffer[i][j];
                            this.gameMatrixBuffer[i][j] = 0;
                        }
                    }
                }
                this.timer = 0;
            }
            else {
                for (var i = 0; i < 20; i++) {
                    for (var j = 0; j < 10; j++) {
                        if (this.gameMatrixBuffer[i][j] != 0) {
                            this.gameMatrix[i][j] = this.gameMatrixBuffer[i][j];
                            this.gameMatrixBuffer[i][j] = 0;
                        }
                    }
                }
                this.toMakePiece = true;
                this.timer = -1;
            }
        };
        MyApp.prototype.rotate = function () {
            if (this.game_mode != GAME_MODE.PLAYING)
                return;
            if (this.toClear)
                return;
            if (this.piece == 1) {
                if (this.state == 1) {
                    if (this.centY != 19 && this.gameMatrix[this.centY + 1][this.centX] == 0) {
                        this.gameMatrixBuffer[this.centY + 1][this.centX] = this.piece;
                        this.gameMatrixBuffer[this.centY][this.centX - 1] = 0;
                        this.state = 2;
                        return;
                    }
                }
                if (this.state == 2) {
                    if (this.centX != 0 && this.gameMatrix[this.centY][this.centX - 1] == 0) {
                        this.gameMatrixBuffer[this.centY][this.centX - 1] = this.piece;
                        this.gameMatrixBuffer[this.centY - 1][this.centX] = 0;
                        this.state = 3;
                        return;
                    }
                }
                if (this.state == 3) {
                    if (this.gameMatrix[this.centY - 1][this.centX] == 0) {
                        this.gameMatrixBuffer[this.centY - 1][this.centX] = this.piece;
                        this.gameMatrixBuffer[this.centY][this.centX + 1] = 0;
                        this.state = 4;
                        return;
                    }
                }
                if (this.state == 4) {
                    if (this.centX != 9 && this.gameMatrix[this.centY][this.centX + 1] == 0) {
                        this.gameMatrixBuffer[this.centY][this.centX + 1] = this.piece;
                        this.gameMatrixBuffer[this.centY + 1][this.centX] = 0;
                        this.state = 1;
                        return;
                    }
                }
            } //this.piece 1
            if (this.piece == 2) {
                if (this.state == 1) {
                    if (this.centY != 19 && this.gameMatrix[this.centY + 1][this.centX] == 0 && this.gameMatrix[this.centY - 1][this.centX - 1] == 0) {
                        this.gameMatrixBuffer[this.centY + 1][this.centX] = this.piece;
                        this.gameMatrixBuffer[this.centY - 1][this.centX - 1] = this.piece;
                        this.gameMatrixBuffer[this.centY - 1][this.centX] = 0;
                        this.gameMatrixBuffer[this.centY - 1][this.centX + 1] = 0;
                        this.state = 2;
                        return;
                    }
                }
                if (this.state == 2) {
                    if (this.centX != 9 && this.gameMatrix[this.centY - 1][this.centX] == 0 && this.gameMatrix[this.centY - 1][this.centX + 1] == 0) {
                        this.gameMatrixBuffer[this.centY - 1][this.centX] = this.piece;
                        this.gameMatrixBuffer[this.centY - 1][this.centX + 1] = this.piece;
                        this.gameMatrixBuffer[this.centY + 1][this.centX] = 0;
                        this.gameMatrixBuffer[this.centY - 1][this.centX - 1] = 0;
                        this.state = 1;
                        return;
                    }
                }
            } //this.piece 2
            if (this.piece == 3) {
                if (this.state == 1) {
                    if (this.centY != 19 && this.gameMatrix[this.centY + 1][this.centX] == 0 && this.gameMatrix[this.centY - 1][this.centX + 1] == 0) {
                        this.gameMatrixBuffer[this.centY + 1][this.centX] = this.piece;
                        this.gameMatrixBuffer[this.centY - 1][this.centX + 1] = this.piece;
                        this.gameMatrixBuffer[this.centY - 1][this.centX] = 0;
                        this.gameMatrixBuffer[this.centY - 1][this.centX - 1] = 0;
                        this.state = 2;
                        return;
                    }
                }
                if (this.state == 2) {
                    if (this.centX != 0 && this.gameMatrix[this.centY - 1][this.centX] == 0 && this.gameMatrix[this.centY - 1][this.centX - 1] == 0) {
                        this.gameMatrixBuffer[this.centY - 1][this.centX] = this.piece;
                        this.gameMatrixBuffer[this.centY - 1][this.centX - 1] = this.piece;
                        this.gameMatrixBuffer[this.centY + 1][this.centX] = 0;
                        this.gameMatrixBuffer[this.centY - 1][this.centX + 1] = 0;
                        this.state = 1;
                        return;
                    }
                }
            } //this.piece 3
            if (this.piece == 4) {
                if (this.state == 1) {
                    if (this.centY != 19 && this.gameMatrix[this.centY - 1][this.centX] == 0 && this.gameMatrix[this.centY + 1][this.centX] == 0 && this.gameMatrix[this.centY + 1][this.centX + 1] == 0) {
                        this.gameMatrixBuffer[this.centY - 1][this.centX] = this.piece;
                        this.gameMatrixBuffer[this.centY + 1][this.centX] = this.piece;
                        this.gameMatrixBuffer[this.centY + 1][this.centX + 1] = this.piece;
                        this.gameMatrixBuffer[this.centY][this.centX - 1] = 0;
                        this.gameMatrixBuffer[this.centY][this.centX + 1] = 0;
                        this.gameMatrixBuffer[this.centY - 1][this.centX + 1] = 0;
                        this.state = 2;
                        return;
                    }
                }
                if (this.state == 2) {
                    if (this.centX != 0 && this.gameMatrix[this.centY][this.centX - 1] == 0 && this.gameMatrix[this.centY + 1][this.centX - 1] == 0 && this.gameMatrix[this.centY][this.centX + 1] == 0) {
                        this.gameMatrixBuffer[this.centY][this.centX - 1] = this.piece;
                        this.gameMatrixBuffer[this.centY + 1][this.centX - 1] = this.piece;
                        this.gameMatrixBuffer[this.centY][this.centX + 1] = this.piece;
                        this.gameMatrixBuffer[this.centY + 1][this.centX] = 0;
                        this.gameMatrixBuffer[this.centY + 1][this.centX + 1] = 0;
                        this.gameMatrixBuffer[this.centY - 1][this.centX] = 0;
                        this.state = 3;
                        return;
                    }
                }
                if (this.state == 3) {
                    if (this.gameMatrix[this.centY - 1][this.centX] == 0 && this.gameMatrix[this.centY - 1][this.centX - 1] == 0 && this.gameMatrix[this.centY + 1][this.centX] == 0) {
                        this.gameMatrixBuffer[this.centY - 1][this.centX] = this.piece;
                        this.gameMatrixBuffer[this.centY - 1][this.centX - 1] = this.piece;
                        this.gameMatrixBuffer[this.centY + 1][this.centX] = this.piece;
                        this.gameMatrixBuffer[this.centY][this.centX - 1] = 0;
                        this.gameMatrixBuffer[this.centY + 1][this.centX - 1] = 0;
                        this.gameMatrixBuffer[this.centY][this.centX + 1] = 0;
                        this.state = 4;
                        return;
                    }
                }
                if (this.state == 4) {
                    if (this.centX != 9 && this.gameMatrix[this.centY][this.centX - 1] == 0 && this.gameMatrix[this.centY][this.centX + 1] == 0 && this.gameMatrix[this.centY - 1][this.centX + 1] == 0) {
                        this.gameMatrixBuffer[this.centY][this.centX - 1] = this.piece;
                        this.gameMatrixBuffer[this.centY][this.centX + 1] = this.piece;
                        this.gameMatrixBuffer[this.centY - 1][this.centX + 1] = this.piece;
                        this.gameMatrixBuffer[this.centY - 1][this.centX] = 0;
                        this.gameMatrixBuffer[this.centY - 1][this.centX - 1] = 0;
                        this.gameMatrixBuffer[this.centY + 1][this.centX] = 0;
                        this.state = 1;
                        return;
                    }
                }
            } //this.piece 4
            if (this.piece == 5) {
                if (this.state == 1) {
                    if (this.centY != 19 && this.gameMatrix[this.centY - 1][this.centX] == 0 && this.gameMatrix[this.centY + 1][this.centX] == 0 && this.gameMatrix[this.centY - 1][this.centX + 1] == 0) {
                        this.gameMatrixBuffer[this.centY - 1][this.centX] = this.piece;
                        this.gameMatrixBuffer[this.centY + 1][this.centX] = this.piece;
                        this.gameMatrixBuffer[this.centY - 1][this.centX + 1] = this.piece;
                        this.gameMatrixBuffer[this.centY][this.centX - 1] = 0;
                        this.gameMatrixBuffer[this.centY][this.centX + 1] = 0;
                        this.gameMatrixBuffer[this.centY - 1][this.centX - 1] = 0;
                        this.state = 2;
                        return;
                    }
                }
                if (this.state == 2) {
                    if (this.centX != 0 && this.gameMatrix[this.centY][this.centX - 1] == 0 && this.gameMatrix[this.centY + 1][this.centX + 1] == 0 && this.gameMatrix[this.centY][this.centX + 1] == 0) {
                        this.gameMatrixBuffer[this.centY][this.centX - 1] = this.piece;
                        this.gameMatrixBuffer[this.centY + 1][this.centX + 1] = this.piece;
                        this.gameMatrixBuffer[this.centY][this.centX + 1] = this.piece;
                        this.gameMatrixBuffer[this.centY + 1][this.centX] = 0;
                        this.gameMatrixBuffer[this.centY - 1][this.centX + 1] = 0;
                        this.gameMatrixBuffer[this.centY - 1][this.centX] = 0;
                        this.state = 3;
                        return;
                    }
                }
                if (this.state == 3) {
                    if (this.gameMatrix[this.centY - 1][this.centX] == 0 && this.gameMatrix[this.centY + 1][this.centX - 1] == 0 && this.gameMatrix[this.centY + 1][this.centX] == 0) {
                        this.gameMatrixBuffer[this.centY - 1][this.centX] = this.piece;
                        this.gameMatrixBuffer[this.centY + 1][this.centX - 1] = this.piece;
                        this.gameMatrixBuffer[this.centY + 1][this.centX] = this.piece;
                        this.gameMatrixBuffer[this.centY][this.centX - 1] = 0;
                        this.gameMatrixBuffer[this.centY + 1][this.centX + 1] = 0;
                        this.gameMatrixBuffer[this.centY][this.centX + 1] = 0;
                        this.state = 4;
                        return;
                    }
                }
                if (this.state == 4) {
                    if (this.centX != 9 && this.gameMatrix[this.centY][this.centX - 1] == 0 && this.gameMatrix[this.centY][this.centX + 1] == 0 && this.gameMatrix[this.centY - 1][this.centX - 1] == 0) {
                        this.gameMatrixBuffer[this.centY][this.centX - 1] = this.piece;
                        this.gameMatrixBuffer[this.centY][this.centX + 1] = this.piece;
                        this.gameMatrixBuffer[this.centY - 1][this.centX - 1] = this.piece;
                        this.gameMatrixBuffer[this.centY - 1][this.centX] = 0;
                        this.gameMatrixBuffer[this.centY + 1][this.centX] = 0;
                        this.gameMatrixBuffer[this.centY + 1][this.centX - 1] = 0;
                        this.state = 1;
                        return;
                    }
                }
            } //this.piece 5
            if (this.piece == 7) {
                if (this.state == 1) {
                    if (this.centX != 0 && this.centX != 1 && this.centX != 9 && this.gameMatrix[this.centY][this.centX - 1] == 0 && this.gameMatrix[this.centY][this.centX - 2] == 0 && this.gameMatrix[this.centY][this.centX + 1] == 0) {
                        this.gameMatrixBuffer[this.centY][this.centX - 1] = this.piece;
                        this.gameMatrixBuffer[this.centY][this.centX - 2] = this.piece;
                        this.gameMatrixBuffer[this.centY][this.centX + 1] = this.piece;
                        this.gameMatrixBuffer[this.centY - 1][this.centX] = 0;
                        this.gameMatrixBuffer[this.centY + 1][this.centX] = 0;
                        this.gameMatrixBuffer[this.centY + 2][this.centX] = 0;
                        this.state = 2;
                        return;
                    }
                }
                if (this.state == 2) {
                    if (this.centY != 19 && this.centY != 18 && this.gameMatrix[this.centY - 1][this.centX] == 0 && this.gameMatrix[this.centY + 1][this.centX] == 0 && this.gameMatrix[this.centY + 2][this.centX] == 0) {
                        this.gameMatrixBuffer[this.centY + 2][this.centX] = this.piece;
                        this.gameMatrixBuffer[this.centY + 1][this.centX] = this.piece;
                        this.gameMatrixBuffer[this.centY - 1][this.centX] = this.piece;
                        this.gameMatrixBuffer[this.centY][this.centX - 1] = 0;
                        this.gameMatrixBuffer[this.centY][this.centX - 2] = 0;
                        this.gameMatrixBuffer[this.centY][this.centX + 1] = 0;
                        this.state = 1;
                        return;
                    }
                }
            } //this.piece 7
        };
        MyApp.prototype.checkMatrix = function () {
            for (var i = 0; i < 20; i++) {
                var temp = true;
                for (var j = 0; j < 10; j++) {
                    if (this.gameMatrix[i][j] == 0)
                        temp = false;
                }
                if (temp) {
                    for (var k = 0; k < 10; k++)
                        this.gameMatrix[i][k] = 8;
                    this.toClear = true;
                }
            }
            if (this.toClear) { }
            //TODO blockclear.play();
            else {
                //TODO dropblock.play();
                this.score++;
            }
        };
        MyApp.prototype.clearBlocks = function () {
            var tempLines = 0;
            for (var i = 0; i < 20; i++) {
                var temp = false;
                for (var j = 0; j < 10; j++) {
                    if (this.gameMatrix[i][j] == 8) {
                        temp = true;
                        for (var k = i; k > 0; k--) {
                            this.gameMatrix[k][j] = this.gameMatrix[k - 1][j];
                        }
                        this.gameMatrix[0][j] = 0;
                    }
                }
                if (temp)
                    tempLines++;
            }
            if (tempLines == 1)
                this.score += 50;
            if (tempLines == 2)
                this.score += 100;
            if (tempLines == 3)
                this.score += 300;
            if (tempLines == 4)
                this.score += 1000;
            this.lines += tempLines;
            this.nextLevel += tempLines;
            if (this.nextLevel >= 10) {
                //TODO newlevel.play();
                this.nextLevel -= 10;
                this.level++;
            }
            else
                ; //TODO blockcleared.play();
        };
        MyApp.prototype.requestNextFrame = function () {
            var app = window.myApp;
            //app.processGamepad();
            app.gameLoop();
            app.newPaint();
            requestAnimationFrame(app.requestNextFrame);
        };
        MyApp.prototype.getMessage = function () {
            return 'New Game';
            // if (!this.draw)
            //     return 'start drawing';
            // else
            //     return 'stop drawing';
        };
        MyApp.prototype.countFPS = function () {
            this.fpscounter++;
            var delta = (new Date().getTime() - this.lastCalledTime.getTime()) / 1000;
            if (delta > 1) {
                this.currentfps = this.fpscounter;
                this.fpscounter = 0;
                this.lastCalledTime = new Date();
                // $("#fps").html(this.currentfps.toString());
            }
        };
        MyApp.prototype.getRandomColor = function () {
            var letters = '0123456789ABCDEF';
            var color = '#';
            for (var i = 0; i < 6; i++) {
                color += letters[Math.floor(Math.random() * 16)];
            }
            return color;
        };
        MyApp.prototype.newPaint = function () {
            var _this = this;
            this.countFPS();
            var randomColor = this.getRandomColor();

            tableHtml = '<pre>';
            for (var y = 0; y < 20; y++) {
                tableHtml += "";
                for (var x = 0; x < 10; x++) {
					var piece = '.';
                    if (_this.gameMatrixBuffer[y][x] == 8 || _this.gameMatrix[y][x] == 8) {
						piece = '.';
                    } else if (_this.gameMatrixBuffer[y][x] > 0 || _this.gameMatrix[y][x] > 0) {
						piece = '#';
					} else {
						piece = '.';
					}
                    tableHtml += piece;
                }
                tableHtml += "\n";
            }
            tableHtml += "</pre>";
            $("#gamediv").html(tableHtml);

/*





            $("[tetris-block]").toArray().forEach(function (element) {
                var x = parseInt(element.attributes["x"].value);
                var y = parseInt(element.attributes["y"].value);
                if (_this.gameMatrixBuffer[y][x] == 8 || _this.gameMatrix[y][x] == 8) {
                    // element.style["background-image"] = 'linear-gradient(-45deg,' + randomColor + ', lightblue)';
                    element.style["background-color"] = randomColor;
                }
                else if (_this.gameMatrixBuffer[y][x] > 0 || _this.gameMatrix[y][x] > 0) {
                    var color = 'blue';
                    // if (this.gameMatrixBuffer[y][x] == 1 || this.gameMatrix[y][x] == 1) color = 'darkblue';
                    // if (this.gameMatrixBuffer[y][x] == 2 || this.gameMatrix[y][x] == 2) color = 'darkorange';
                    // if (this.gameMatrixBuffer[y][x] == 3 || this.gameMatrix[y][x] == 3) color = 'rgb(90, 34, 107)';
                    // if (this.gameMatrixBuffer[y][x] == 4 || this.gameMatrix[y][x] == 4) color = 'darkred';
                    // if (this.gameMatrixBuffer[y][x] == 5 || this.gameMatrix[y][x] == 5) color = 'darkcyan';
                    // if (this.gameMatrixBuffer[y][x] == 6 || this.gameMatrix[y][x] == 6) color = 'darkgreen';
                    // if (this.gameMatrixBuffer[y][x] == 7 || this.gameMatrix[y][x] == 7) color = 'rgb(209, 209, 0)';
                    // element.style["background-image"] = 'linear-gradient(-45deg,blue, lightblue)';
                    element.style["background-color"] = color;
                }
                else if (_this.shadowFinderMatrix && _this.shadowFinderMatrix[y][x] > 0) {
                    // element.style["background-image"] = 'linear-gradient(grey, grey)';
                    element.style["background-color"] = 'grey';
                }
                else {
                    // element.style["background-image"] = 'linear-gradient(white, white)';
                    element.style["background-color"] = 'white';
                }
            });
*/
            //draw next piece
/*
            $("[nextpiece-block]").toArray().forEach(function (element) {
                var x = parseInt(element.attributes["x"].value);
                var y = parseInt(element.attributes["y"].value);
                if (_this.nextPieceMatrix[y][x] > 0)
                    element.style["background-color"] = 'blue';
                else
                    element.style["background-color"] = 'white';
            });
*/
        };
        MyApp.prototype.btnClick = function () {
            this.reset();
        };
        MyApp.prototype.btnPause = function () {
            if (this.game_mode == GAME_MODE.TITLE)
                return;
            if (this.game_mode == GAME_MODE.PAUSED)
                this.game_mode = GAME_MODE.PLAYING;
            else
                this.game_mode = GAME_MODE.PAUSED;
        };
        MyApp.prototype.getPauseButtonText = function () {
            if (this.game_mode == GAME_MODE.PAUSED)
                return "Resume";
            else
                return "Pause";
        };
        MyApp.prototype.createGameTable = function () {
            var tableHtml = '';
            //var boxSize = 20;
            //var windowHeight = window.innerHeight;
            //var windowWidth = window.innerWidth;
            // console.log(windowHeight,windowWidth);
            //if (windowHeight > 800) {
            //    var extraSpace = windowHeight - 800;
            //    extraSpace = extraSpace / 20;
            //    boxSize += extraSpace;
            //}
            //tableHtml += '<table style="margin: 0px auto;">';
            tableHtml += '<pre>';
            for (var i = 0; i < 20; i++) {
                tableHtml += "";
                for (var j = 0; j < 10; j++) {
                    var piece = '#';
                    tableHtml += piece;
                }
                tableHtml += "\n";
            }
            tableHtml += "</pre>";
            $("#gamediv").html(tableHtml);
 /*
           var nextPieceHtml = '<table>';
            for (var i = 0; i < 4; i++) {
                nextPieceHtml += "<tr>";
                for (var j = 0; j < 5; j++) {
                    var piece = '#';
                    nextPieceHtml += "<td nextpiece-block x='" + j + "' y='" + i +
                        "' style='width:1em;height:1em;background-color:white;" +
                        "'>" + piece + "</td>";
                }
                nextPieceHtml += "</tr>";
            }
            nextPieceHtml += "</table>";
            $("#nextpiecediv").html(nextPieceHtml);
*/
            // let gameDivHeight:number = $("#gamediv").height();
            // console.log(gameDivHeight,windowHeight);
            // $("#gamediv")[0].style["margin-top"] = (windowHeight-gameDivHeight-140) + 'px';
        };
        return MyApp;
    }());
    exports.MyApp = MyApp;
    window.myApp = new MyApp();
});
//# sourceMappingURL=index.js.map