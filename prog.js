const fb = require('fb');
const st = require('Storage');

const icon = st.readArrayBuffer('icon-timer.i');

fb.init();
// fb.add({ x: 0, y: 0, w: 240, h: 240, c: 0xffff })
fb.add({ x: 50, y: 50, w: 140, h: 140, c: fb.color(50, 50, 0) });
fb.add({ x: 25, y: 150, w: 50, h: 50, c: fb.color(255, 0, 0) });
fb.add({ x: 150, y: 25, w: 50, h: 50, c: fb.color(0, 0, 255) });
fb.add({ x: 150, y: 150, w: 50, h: 50, c: fb.color(0, 255, 0) });
fb.add({ x: 25, y: 25, w: 80, h: 80, data: icon, c: fb.color(0, 255, 255) });

const BL = D31;
const MOSI = D3;
const SCK = D4;
const DC = D29;
const CS = D28;
const RST = D30;

function init(spi, dc, ce, rst, callback) {
  function cmd(c, d) {
    dc.reset();
    spi.write(c, ce);
    if (d !== undefined) {
      dc.set();
      spi.write(d, ce);
    }
  }

  digitalPulse(rst, 0, 10);

  const ST7789_INIT_CODE = [
    [0x11, 0],     //SLPOUT (11h):
    [0x36, 0],     // MADCTL
    [0x3A, 0x55],  // COLMOD - interface pixel format - 16bpp
    [0xB2, [0xC, 0xC, 0, 0x33, 0x33]], // PORCTRL (B2h): Porch Setting
    [0xB7, 0],     // GCTRL (B7h): Gate Control
    [0xBB, 0x3E],  // VCOMS (BBh): VCOM Setting 
    [0xC2, 1],     // VDVVRHEN (C2h): VDV and VRH Command Enable
    [0xC3, 0x19],  // VRHS (C3h): VRH Set 
    [0xC4, 0x20],  // VDVS (C4h): VDV Set
    [0xC5, 0xF],   // VCMOFSET (C5h): VCOM Offset Set .
    [0xD0, [0xA4, 0xA1]],   // PWCTRL1 (D0h): Power Control 1 
    [0xe0, [0x70, 0x15, 0x20, 0x15, 0x10, 0x09, 0x48, 0x33, 0x53, 0x0B, 0x19, 0x15, 0x2a, 0x2f]],   // PVGAMCTRL (E0h): Positive Voltage Gamma Control
    [0xe1, [0x70, 0x15, 0x20, 0x15, 0x10, 0x09, 0x48, 0x33, 0x53, 0x0B, 0x19, 0x15, 0x2a, 0x2f]],   // NVGAMCTRL (E1h): Negative Voltage Gamma Contro
    [0x29, 0], // DISPON (29h): Display On 
    [0x21, 0], // INVON (21h): Display Inversion On
    [0, 0]// 255/*DATA_LEN = 255 => END*/
  ];

  setTimeout(function () {
    cmd(0x11); //Exit Sleep
    setTimeout(function () {
      ST7789_INIT_CODE.forEach(function (e) {
        cmd(e[0], e[1])
      });
      if (callback) callback();
    }, 20);
  }, 120);
}

BL.set(); // LCD backlight on
var spi = SPI1;
spi.setup({ mosi: MOSI, sck: SCK, baud: 8 * 1024 * 1024 });
init(spi, DC, CS, RST, () => {
  CS.reset();
  fb.cmd(spi, [0x2A, 0, 0, 0, 240], 1, DC);
  fb.cmd(spi, [0x2B, 0, 0, 0, 240], 1, DC);

  const d1 = new Date().getTime();
  fb.cmd(spi, [0x2C], 1, DC);
  fb.send(spi, 0);
  const d2 = new Date().getTime();
  console.log(d2 - d1, process.memory().free);
  CS.set();
})
