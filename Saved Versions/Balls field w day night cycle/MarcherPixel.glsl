#version 460 core

layout(local_size_x = 1, local_size_y = 1) in;
layout(rgba32f, binding = 0) uniform image2D imageOutput;

//The 'background color' or 'sky color' of the scene
uniform vec3 fillColor;

//If the screen is 1 unit away from the camera, this number is the vertical distance from the top of the screen to the camera.
uniform float screenTop;

//The transformation matrix of the camera.
//Used to move coordinates from world space to camera space so the SDFs work right.
uniform mat4 viewMatrixInv;

//The direction an orthographic light is shining from
//uniform vec3 lightDirection;
const vec3 lightDirection_world = vec3(0, 0, -1);

//Variable defining the scene
//This has to be done in the shader for now.
const vec3 spherePosition_world = vec3(0.0f, 0.0f, 0.0f);
const float sphereRadius = 1.0f;

//Diffuse color of the sphere
//uniform vec3 sphereDiffuseColor;
const vec3 sphereDiffuseColor = vec3(0.8, 0, 0);

//Blinn Phong exponent of the sphere
//uniform float sphereBlinnPhong;
const float sphereBlinnPhong = 70.0f;

void main() {
	//The first step is to find what ray this call is dealing with
	ivec2 pixelCoords = ivec2(gl_GlobalInvocationID.xy);	//The coordinates of the pixel that this shader call is drawing
	ivec2 imageDimensions = imageSize(imageOutput);			//The total size, in pixels, of the image the whole shader is drawing
	
	//TODO: Move this to the main program to improve performace?
	float aspectRatio = float(imageDimensions.x) / float(imageDimensions.y);
	float screenRight = aspectRatio * float(screenTop);
	float distancePerPixel = 2 * screenTop / float(imageDimensions.y);

	vec3 rayDirection_view = vec3(-screenRight + pixelCoords.x * distancePerPixel, -screenTop + pixelCoords.y * distancePerPixel, -1);
	vec3 rayDirection_world = normalize(vec3(viewMatrixInv * vec4(rayDirection_view, 0)));
	vec3 rayOrigin_world = vec3(viewMatrixInv * vec4(0, 0, 0, 1));

	//Now we actually march towards the sphere.
	//This is done iteratively instead of recursively to save memory.
	float sdfValue;
	vec3 moddedPoint;
	vec3 normal;
	vec3 toEye;
	vec3 toLight = -normalize(lightDirection_world);
	vec3 testPoint = rayOrigin_world;
	while(true){
		//Calculates sphere sdf
		moddedPoint = vec3(
			mod(testPoint.x + 2, 4) - 2,
			testPoint.y,
			mod(testPoint.z + 2, 4) - 2
		);
		sdfValue = length(moddedPoint - spherePosition_world) - sphereRadius;

		if((sdfValue) < 0.001){
			//Calculate the normal of the point that was hit
			normal = normalize(moddedPoint - spherePosition_world);
			toEye = normalize(rayOrigin_world - testPoint);
			vec3 halfway = normalize(toEye + toLight);
			imageStore(imageOutput, pixelCoords, vec4(vec3(1, 0, 0) * dot(normal, toLight) + vec3(1, 1, 1) * pow(clamp(dot(halfway, normal), 0, 1), 70), 1));
			//imageStore(imageOutput, pixelCoords, vec4(1, 0, 0, 1));
			return;
		}

		if(length(testPoint - rayOrigin_world) > 1000){
			break;
		}

		//Steps the test point along the ray
		testPoint += sdfValue * rayDirection_world;
	}
	imageStore(imageOutput, pixelCoords, vec4(fillColor, 1));
}