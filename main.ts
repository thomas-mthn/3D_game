"use strict"

let code_keycode_table: Record<string, number> = {
	'Digit0': 48, 'Digit1': 49, 'Digit2': 50, 'Digit3': 51, 'Digit4': 52,
	'Digit5': 53, 'Digit6': 54, 'Digit7': 55, 'Digit8': 56, 'Digit9': 57,
	'KeyA': 65, 'KeyB': 66, 'KeyC': 67, 'KeyD': 68, 'KeyE': 69,
	'KeyF': 70, 'KeyG': 71, 'KeyH': 72, 'KeyI': 73, 'KeyJ': 74,
	'KeyK': 75, 'KeyL': 76, 'KeyM': 77, 'KeyN': 78, 'KeyO': 79,
	'KeyP': 80, 'KeyQ': 81, 'KeyR': 82, 'KeyS': 83, 'KeyT': 84,
	'KeyU': 85, 'KeyV': 86, 'KeyW': 87, 'KeyX': 88, 'KeyY': 89, 'KeyZ': 90,
		
	'Backquote': 192,     
	'Minus': 189,      
	'Equal': 187,     
	'BracketLeft': 219,
	'BracketRight': 221, 
	'Backslash': 220,   
	'Semicolon': 186,  
	'Quote': 222,        
	'Comma': 188,      
	'Period': 190,   
	'Slash': 191,    
	
	'Enter': 13,
	'Space': 32,
	'Backspace': 8,
	'Tab': 9,
	'Escape': 27,
	'Delete': 46,
	'CapsLock': 20,
	'ShiftLeft': 16,
	'ShiftRight': 16,
	'ControlLeft': 17,
	'ControlRight': 17,
	'AltLeft': 18,
	'AltRight': 18,
	'MetaLeft': 91,   
	'MetaRight': 92,    
	'ContextMenu': 93,  
		
	'ArrowLeft': 37,
	'ArrowUp': 38,
	'ArrowRight': 39,
	'ArrowDown': 40,
	'Home': 36,
	'End': 35,
	'PageUp': 33,
	'PageDown': 34,
		
	'F1': 112, 'F2': 113, 'F3': 114, 'F4': 115, 'F5': 116,
	'F6': 117, 'F7': 118, 'F8': 119, 'F9': 120, 'F10': 121,
	'F11': 122, 'F12': 123,
		
	'Numpad0': 96, 'Numpad1': 97, 'Numpad2': 98, 'Numpad3': 99,
	'Numpad4': 100, 'Numpad5': 101, 'Numpad6': 102, 'Numpad7': 103,
	'Numpad8': 104, 'Numpad9': 105,
	'NumpadMultiply': 106,
	'NumpadAdd': 107,     
	'NumpadSubtract': 109,
	'NumpadDecimal': 110,
	'NumpadDivide': 111,  
	'NumpadEnter': 13,
	'NumLock': 144,
		
	'ScrollLock': 145,
	'Pause': 19,        
		
	'MediaTrackPrevious': 177,
	'MediaTrackNext': 176,
	'MediaPlayPause': 179,
	'MediaStop': 178,
	'VolumeMute': 173,
	'VolumeDown': 174,
	'VolumeUp': 175
};

let memory: any;

let wasm_frame: (tick: number) => number;
let wasm_init: (voxel_serial_size: number,width: number,height: number) => void;
let wasm_keydown: (key: number) => void;
let wasm_left_button_down: () => void;
let wasm_right_button_down: () => void;
let wasm_middle_button_down: () => void;
let wasm_mousemove: (delta_x: number,delta_y: number) => void;

let key_array;
let g_wasm_world_data: number;
let g_key: number;
let instance;
let prev_tick = performance.now();
let width = 2560 / 2;
let height = 1440 / 2;
let canvas = document.getElementById("canvas") as HTMLCanvasElement;

type ShaderProgram = {
    vertex_shader: WebGLShader;
    fragment_shader: WebGLShader;
    id: WebGLProgram;
    vao?: WebGLVertexArrayObject;
};

function shaderProgramCreate(gl: WebGL2RenderingContext,vertex_source: string,fragment_source: string) : ShaderProgram{
	let program: ShaderProgram = {
		id: gl.createProgram()!,
		vertex_shader: gl.createShader(gl.VERTEX_SHADER)!,
		fragment_shader:  gl.createShader(gl.FRAGMENT_SHADER)!,
	};

    gl.shaderSource(program.vertex_shader,vertex_source);
    gl.shaderSource(program.fragment_shader,fragment_source);
    gl.compileShader(program.vertex_shader);
    gl.compileShader(program.fragment_shader);
    gl.attachShader(program.id,program.vertex_shader);
    gl.attachShader(program.id,program.fragment_shader);
    gl.linkProgram(program.id);
    gl.useProgram(program.id);
    
	return program;
}

let gl: WebGL2RenderingContext;

function frame(){
	let t = performance.now();
	let ptr = wasm_frame(performance.now());
	let pixel_data = new Uint8ClampedArray(memory.buffer,ptr,width * height * 4);
	let imageData = new ImageData(pixel_data,width,height);
	let ctx = canvas.getContext("2d")!;
	ctx.putImageData(imageData,0,0);


    /*
    let triangle_data = new Uint8Array(memory.buffer,g_wasm_world_data,0x1000);
    let view = new DataView(triangle_data.buffer);

    gl.clearColor(0.0, 0.0, 1.0, 1.0);
    gl.clear(gl.COLOR_BUFFER_BIT);
    
    for(let i = 0;i < view.getInt32(0,true);i++){
        let positions = [
            0, 0,1, 0, 0,0,
            0, 0.5,1, 0, 0.5,0,
            0.7, 0,1, 0.7, 0,0,            
        ];
        gl.bufferData(gl.ARRAY_BUFFER,new Float32Array(positions),gl.STATIC_DRAW);
        gl.drawArrays(gl.TRIANGLES,0,6);
    }
    */
	requestAnimationFrame(frame);
}

let loaded_counter = 0
let world_data_file: Uint8Array;

type Texture = {
    path: string;
    data?: Uint8Array;
};

let textures: Texture[] = [
    {path: "img/grass.bmp"},
    {path: "img/planks.bmp"},
    {path: "img/brick_alt2.bmp"}
];

function loadedInit(){
    console.log("loaded");
    if(loaded_counter < textures.length){
        loaded_counter += 1;
        return;
    }
    console.log("loaded full");
    
    let world_data_size = world_data_file.length;

    for(let texture of textures){
        world_data_size += texture.data!.length;
        world_data_size += texture.path.length;
    }
    
	let world_data = new Uint8Array(memory.buffer,g_wasm_world_data,world_data_size);

    for(let i = 0;i < world_data_file.length;i++)
		world_data[i] = world_data_file[i];

    let offset = world_data_file.length;
    
    for(let texture of textures){
        for(let i = 0;i < texture.path.length;i++)
            world_data[offset++] = texture.path.charCodeAt(i);
    
        for(let i = 0;i < texture.data!.length;i++)
            world_data[offset++] = texture.data![i];
    }
    
	wasm_init(world_data_file.length,width,height);
	canvas.width = width;
	canvas.height = height;
	frame();
}

async function init(){
	try {
		let response = await fetch('test.wasm');
		let bytes = await response.arrayBuffer();
		let result = await WebAssembly.instantiate(bytes, {});
		instance = result.instance; 
		wasm_init = instance.exports.wasmInit as any;
		wasm_frame = instance.exports.wasmFrame as any;
		wasm_keydown = instance.exports.wasmKeyDown as any;
		memory = instance.exports.memory as any;
		g_key = instance.exports.g_key as any;
		g_wasm_world_data = instance.exports.g_wasm_world_data as any;
		wasm_left_button_down   = instance.exports.wasmLeftButtonDown as any;
		wasm_middle_button_down = instance.exports.wasmMiddleButtonDown as any;
		wasm_right_button_down  = instance.exports.wasmRightButtonDown as any;
		wasm_mousemove = instance.exports.wasmMouseMove as any;
		key_array = new Uint8Array(memory.buffer,g_key,0x100);

        let request = new XMLHttpRequest();
        request.open("GET","world/world_1.octvxl");
        request.responseType = "arraybuffer";
        request.onload = () => {
            world_data_file = new Uint8Array(request.response);
            loadedInit();
        };
        request.send();

        for(let texture of textures){
            let request_texture = new XMLHttpRequest();
            request_texture.open("GET",texture.path);
            request_texture.responseType = "arraybuffer";
            request_texture.onload = () => {
                texture.data = new Uint8Array(request_texture.response);
                loadedInit();
            };
            request_texture.send(); 
        } 
	}
	catch(error){
		console.error("Error: ",error);
	}

    let vertex_lighting_source =
        `#version 300 es
    precision mediump float;
    layout (location = 0) in vec3 verticles;
    layout (location = 1) in vec3 lighting;

    out vec3 lighting_io;

    void main(){
	lighting_io = lighting;
	gl_Position = vec4(verticles.xy,0.0,verticles.z);
    }`;

    let fragment_lighting_source =
        `#version 300 es
    precision mediump float;
    out vec4 FragColor;

    in vec3 lighting_io;

    void main(){
FragColor = vec4(lighting_io,1.0);
//FragColor = vec4(1.0,1.0,1.0,1.0); 
    }`;
    
    let canvas_gl = document.getElementById("canvas_gl") as HTMLCanvasElement;
    gl = canvas_gl.getContext("webgl2") as WebGL2RenderingContext;

    let program = shaderProgramCreate(gl,vertex_lighting_source,fragment_lighting_source);

    let buffer = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER,buffer);
    
    let v_pos = gl.getAttribLocation(program.id,"verticles");
    let v_lum = gl.getAttribLocation(program.id,"lighting");

    gl.enableVertexAttribArray(v_pos);
    gl.enableVertexAttribArray(v_lum);
    
    gl.vertexAttribPointer(v_pos,3,gl.FLOAT,false,24,0);
    gl.vertexAttribPointer(v_lum,3,gl.FLOAT,false,24,12);
}

init();
let suppress_mouse_after_pointerlock: boolean;

canvas.addEventListener("click",() => {
	if(document.pointerLockElement === canvas)
		return;
	suppress_mouse_after_pointerlock = true;
	canvas.requestPointerLock( {unadjustedMovement: true});
});

window.addEventListener("keydown",(e) => {
	let key = code_keycode_table[e.code];
	key_array[key] = 0x80;
	wasm_keydown(key);
});

window.addEventListener("keyup",(e) => {
	key_array[code_keycode_table[e.code]] = 0x00;
});

window.addEventListener("mousedown",(e) => {
	if(suppress_mouse_after_pointerlock || document.pointerLockElement != canvas){
		suppress_mouse_after_pointerlock = false;
		return;
	}
	switch(e.button){
		case 0:
			wasm_left_button_down();
			break;
		case 1:
			wasm_middle_button_down();
			break;
		case 2:
			wasm_right_button_down();
			break;
	}
});

window.addEventListener("mousemove",(e) => {
	if(document.pointerLockElement === canvas){
		let dx = e.movementX;
		let dy = e.movementY;
		wasm_mousemove(dx,dy);
	}
});
