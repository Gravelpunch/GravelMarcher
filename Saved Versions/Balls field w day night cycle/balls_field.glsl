#version 460 core

#define PI 3.1415926535897932384626433832795

#define STOP_MODE_TOO_FAR 0
#define STOP_MODE_MAX_ITERS 1
#define STOP_MODE_CLOSE_ENOUGH 2

in vec3 rayView;

//-----------------------------Shader uniforms------------------------------------------

//A 4x4 matrix representing the affine transformation from camera space to world space.
//This should move the point (0, 0, 0) to the camera position, as well as apply any rotations.
//It should also not contain any scaling component. That would break everything.
uniform mat4 matCameraToWorld;

//The direction pointing towards the sun. 
//This should be normalized before being passed here, otherwise everything will look wrong.
uniform vec3 sunDirection;

//The color of the sky. Changes with time of day, etc. Same as the fog color.
uniform vec3 skyColor;


//Simply outputs the rgb-normalized color of this pixel.
out vec3 color;

//--------------------------------Shader variables--------------------------------------

//Calculates the direction of the ray in world space
//This vector represents a direction, and has a length of 1.
vec3 rayWorld;

//The point that the SDF is 'sampled' at. Changes as the ray marches forward. Starts at the camera position
vec3 testPoint;

//The point that the SDF is sampled at, modified to reflect the deformation of space.
vec3 testPointDeformed;

vec3 cameraPosition;

int i;


//----------------------------------Shader technical constants-----------------------------------

//How close the ray has to march to the SDF until it's considered 'on' it.
const float camRayCloseEnough = 0.001f;

//How far from the camera the ray stops marching. (This should also be when the 'fog' hits 1)
const float camRayTooFar = 1000.0f;

//The maximum number of marching steps the SLDF can take before it just gives the sky color
const uint camRayMaxSteps = 4000;

//The maximum number of steps to take while shadow marching. Should be much lower than the actual max steps.
const uint shadowRayMaxSteps = 100;

const float shadowRayTooFar = 100.0f;

//---------------------------------Shader geometry constants----------------------------

const float sphereRadius = 1.0f;
const vec3 sphereCenter = vec3(2, 0, 2);

const vec3 floorNormal = vec3(0, 1, 0);
const float floorOffset = -1.0f;

//--------------------------------Shader lighting constants-------------------------------

const vec3 ambientLight = vec3(0.03, 0.03, 0.03);

const float sphereShininess = 64;
const float floorShininess = 32;

//This one's a bit weird. It's like the shininess value for blinn-phong specularity,
//but in this case controls the width/falloff of the sun in the sky.
const float sunShininess = 1024;

//The sun color is multiplied by this right before being added to the sky color.
//It controls how much of the sun is 'solid' sunColor.
const float sunOverSat = 2;

//-----------------------------Shader colors---------------------------------------

const vec3 sphereDiffuse = vec3(1, 0, 0);
const vec3 sphereSpecular = vec3(1, 1, 1);

const vec3 floorDiffuse = vec3(0, .8, .2);
const vec3 floorSpecular = vec3(1, 1, 1);

const vec3 sunColor = vec3(1, 1, 0.85);

vec3 deformation(vec3 p) {
	//return p;
	return mod(p, vec3(4, 0, 4));
}

//This is a function that determines how the fog falls off with distance.
//Different functions can give very different feels to a scene.
float fogFalloff(float d) {
	return d;
}

//-----------------------------------SDF Output variables---------------------------------------
//These are modified (really set) as the SDF evaluates, then other parts of the program can look at them to see what's up

//The actual value of the SDF. Represents a lower bound on the distance from the point the SDF was evaluated at, to the nearest geometry.
//The sign is significant. Positive means the point is outside an object, negative means inside, 0 means it's on a surface.
float sdfValue;

//The normal of the surface closest to the point the SDF was evaluated at.
vec3 surfaceNormal;

//The diffuse color of the surface closest to the point the SDF was evaluated at.
vec3 surfaceDiffuse;

//The specular color of blah blah blah
vec3 surfaceSpecular;

//The specular exponent (shininess) of the surface
float surfaceShininess;

//The actual SDF. This is where the real meat of the scene is. By changing this function, the whole scene can be changed.
void sdf(vec3 p, bool includeColorCalcs) {
	
	//First applies the space deformation.
	vec3 pp = deformation(p);

	vec3 ppc = pp - sphereCenter;
	float sdfSphereValue = length(ppc) - sphereRadius;
	float sdfPlaneValue = dot((pp - floorOffset * floorNormal), floorNormal);

	if(sdfSphereValue < sdfPlaneValue) {
		sdfValue = sdfSphereValue;
		if(!includeColorCalcs) return;

		//The coloring data. Only needed if includeColorCalcs was true.
		surfaceNormal = normalize(ppc);
		surfaceDiffuse = sphereDiffuse;
		surfaceSpecular = sphereSpecular;
		surfaceShininess = sphereShininess;

	} else {
		sdfValue = sdfPlaneValue;
		if(!includeColorCalcs) return;

		surfaceNormal = floorNormal;
		surfaceDiffuse = floorDiffuse;
		surfaceSpecular = floorSpecular;
		surfaceShininess = floorShininess;
	}

}

//---------------------------march output variables----------------------------------
//These are set each time the march function is run.

//This is the where the test point was when the function stopped, whether because it ran out of iterations or because it got close enough.
vec3 marchEndPoint;

//This represents the reason the marching stopped.
uint marchStopMode;

//This represents how many iterations were done before the marching stopped, for whatever reason
uint marchIterCount;

//The actual marching function.
void march(vec3 origin, vec3 direction, float closeEnough, float tooFar, uint maxIterations, bool includeColorCalcs){
	marchEndPoint = origin;
	for(marchIterCount = 0; marchIterCount < maxIterations; marchIterCount++){
		
		//Finds the SDF value
		sdf(marchEndPoint, includeColorCalcs);
		
		//If the test point hit the surface
		if(sdfValue < closeEnough){
			//iterCount and marchEndPoint are already set.
			marchStopMode = STOP_MODE_CLOSE_ENOUGH;
			return;
		}

		//Marches the point forward
		marchEndPoint += sdfValue * direction;

		//If the test point is too far
		if(length(marchEndPoint - origin) > tooFar) {
			marchStopMode = STOP_MODE_TOO_FAR;
			return;
		}
	}
	marchStopMode = STOP_MODE_MAX_ITERS;
}

//The main function assembles and coordinates all the other functions to actually draw colors.
void main() {
	
	//The ray for this pixel is the normalized, interpolated ray from the vertices of the screen quad.
	rayWorld = normalize((matCameraToWorld * vec4(rayView, 0)).xyz);

	//Finds the camera position
	cameraPosition = (matCameraToWorld * vec4(0, 0, 0, 1)).xyz;

	//Does the initial camera-ray marching.
	march(cameraPosition, rayWorld, camRayCloseEnough, camRayTooFar, camRayMaxSteps, true);

	//If the ray ended because it got too far or hit max iters, draw the sky.
	if(marchStopMode == STOP_MODE_TOO_FAR) {
		color = 
			skyColor +
			pow(max(dot(sunDirection, rayWorld), 0.0f), sunShininess) * sunOverSat
		;
		return;
	}

	//Otherwise, it hit an object.
	//Save the end point (the point it hit) and the color data
	vec3 camRayHitPoint = marchEndPoint;
	vec3 camRayHitNormal = surfaceNormal;
	vec3 camRayHitDiffuse = surfaceDiffuse;
	vec3 camRayHitSpecular = surfaceSpecular;
	float camRayHitShininess = surfaceShininess;

	//Determine if the hit point is in shadow or not.
	//Assume it is unless shown otherwise.
	bool isInShadow = true;

	//Only shadow march if the object isn't shadowing itself, i.e, its normal is facing away from the sun.
	if(dot(camRayHitNormal, sunDirection) > 0) {
		march(camRayHitPoint + sunDirection * 0.01, sunDirection, 0.0, shadowRayTooFar, shadowRayMaxSteps, false);
		//If the ray made it to 'infinity,' we know the object is lit. This is the only case in which it's lit.
		if(marchStopMode == STOP_MODE_TOO_FAR){
			isInShadow = false;
		}
	}

	//Only apply the non-ambient light if the point isn't in shadow.
	vec3 lightingComponent = vec3(0, 0, 0);
	if(!isInShadow) {
		vec3 halfway = normalize(normalize(cameraPosition - camRayHitPoint) + sunDirection);
		lightingComponent = 
			/*The diffuse component*/	camRayHitDiffuse * max(dot(camRayHitNormal, sunDirection), 0.0f) + 
			/*The specular component*/	camRayHitSpecular * pow(max(dot(halfway, camRayHitNormal), 0.0f), camRayHitShininess)
		;
	}

	color = mix(
		camRayHitDiffuse * ambientLight + lightingComponent,
		skyColor,
		fogFalloff(length(cameraPosition - camRayHitPoint) / camRayTooFar)
	);

}