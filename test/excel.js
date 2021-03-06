require('../activex');

var path = require('path'); 
const assert = require('assert');

var template_filename = path.join(__dirname, '../data/test.xltm');
var excel, wbk;

var test_value = 'value';
var test_value2 = 'value2';
var test_value3 = 'value3';
var test_func_arg = 10;
var com_obj, js_obj = {
    text: test_value,
    obj: { params: test_value },
    arr: [ test_value, test_value, test_value ],
    func: function(v) { return v*2; }
};

describe("COM from JS object", function() {

    it("create", function() {
        com_obj = new ActiveXObject(js_obj);
    });

    it("read simple property", function() {
        if (com_obj) assert.equal(com_obj.text, js_obj.text);
    });

    it("read object property", function() {
        if (com_obj) assert.equal(com_obj.obj.params, js_obj.obj.params);
    });

    it("read array property", function() {
        if (com_obj) {
            assert.equal(com_obj.arr.length, js_obj.arr.length);
            assert.equal(com_obj.arr[0], js_obj.arr[0]);
        }
    });

    it("change simple property", function() {
        if (com_obj) {
            com_obj.text = test_value2;
            assert.equal(com_obj.text, test_value2);
            assert.equal(js_obj.text, test_value2);
        }
    });

    it("change object property", function() {
        if (com_obj) {
            com_obj.obj.params = test_value2;
            assert.equal(com_obj.obj.params, test_value2);
            assert.equal(js_obj.obj.params, test_value2);
        }
    });

    it("change array property", function() {
        if (com_obj) {
            com_obj.arr[0] = test_value2;
            assert.equal(com_obj.arr[0], test_value2);
            assert.equal(js_obj.arr[0], test_value2);
        }
    });

    it("call method", function() {
        if (com_obj) assert.equal(com_obj.func(test_func_arg), js_obj.func(test_func_arg));
    });

});

describe("Excel with JS object", function() {

    it("create", function() {
        excel = new ActiveXObject("Excel.Application", { activate: true });
    });

    it("create workbook from test template", function() {
        if (excel) wbk = excel.Workbooks.Add(template_filename);
    });

    it("invoke test simple property", function() {
        if (wbk && com_obj) assert.equal(test_value3, wbk.Test(com_obj, 'text', 0, test_value3));
    });

    it("invoke test object property", function() {
        if (wbk && com_obj) assert.equal(test_value3, wbk.Test(com_obj, 'obj', 0, test_value3));
    });

    it("invoke test array property", function() {
        if (wbk && com_obj) assert.equal(test_value3, wbk.Test(com_obj, 'arr', 0, test_value3));
    });

    it("invoke test method", function() {
        if (wbk && com_obj) assert.equal(js_obj.func(test_func_arg), wbk.Test(com_obj, 'func', 0, test_func_arg));
    });

    it("quit", function() {
        if (wbk) wbk.Close(false);
        if (excel) excel.Quit();
    });

});