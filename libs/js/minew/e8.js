/// Returns a LIS3DH instance. callback when initialised. Then use 'read' to get data
exports.setAccelOn = function (isOn, callback) {
    if (this.LIS3DH) this.LIS3DH.off();
    delete this.LIS3DH;
    if (isOn) {
        var i2c = new I2C();
        i2c.setup({ sda: D12, scl: D11 });
        if (callback) setTimeout(callback, 100, this.LIS3DH); // wait for first reading
        // {int:pin} isn't used yet, but at some point the module might include support
        return this.LIS3DH = require("LIS3DH").connectI2C(i2c, { int: D07 });
    }
};

E.on('init', function () {
});
