#import "AppDelegate.h"

#import <FillExtrusionShadows/FillExtrusionShadows.h>
#import <GltfLayer/GltfLayer.h>
#import <RectangleLayer/RectangleLayer.h>
#import "MLNMapCamera.h"
#import "MLNMapView.h"
#import "MLNMapViewDelegate.h"
#import "MLNStyle.h"

@interface AppDelegate () <MLNMapViewDelegate>
@property(nonatomic, strong) MLNMapView *mapView;
@property(nonatomic, strong) UIButton *shadowToggle;
@property(nonatomic, strong) UIButton *sceneToggle;
@property(nonatomic) BOOL shadowEnabled;
@property(nonatomic) BOOL showingModel;
@property(nonatomic) BOOL showingRectangles;
@end

@implementation AppDelegate

- (void)mapView:(MLNMapView *)mapView didFinishLoadingStyle:(MLNStyle *)style {
    MLNStyleLayer *modelLayer = [style layerWithIdentifier:@"eiffel-tower-gltf"];
    MLNStyleLayer *rectangleLayer = [style layerWithIdentifier:@"points"];
    NSLog(@"Loaded style with %lu layers; GLTF Objective-C wrapper %@",
          (unsigned long)style.layers.count,
          modelLayer ? @"is present" : @"is missing");
    CLLocationCoordinate2D center = modelLayer ? CLLocationCoordinate2DMake(48.8582621, 2.2944962)
                                    : rectangleLayer ? CLLocationCoordinate2DMake(48.8566, 2.3522)
                                                     : CLLocationCoordinate2DMake(52.5096, 13.3760);
    mapView.camera = [MLNMapCamera cameraLookingAtCenterCoordinate:center
                                                          altitude:modelLayer ? 650 : rectangleLayer ? 1200000 : 850
                                                             pitch:modelLayer ? 62 : rectangleLayer ? 0 : 52
                                                           heading:modelLayer ? 28 : rectangleLayer ? 0 : 20];
}

- (NSURL *)shadowStyleURLWithShadowEnabled:(BOOL)enabled {
    NSURL *source = [[NSBundle mainBundle] URLForResource:@"liberty-shadow" withExtension:@"json"];
    NSAssert(source, @"The bundled Liberty shadow style is missing");
    if (enabled) return source;

    NSData *data = [NSData dataWithContentsOfURL:source];
    NSMutableDictionary *style = [NSJSONSerialization JSONObjectWithData:data
                                                                  options:NSJSONReadingMutableContainers
                                                                    error:nil];
    for (NSMutableDictionary *layer in style[@"layers"]) {
        if ([layer[@"type"] isEqualToString:@"fill-extrusion"]) {
            NSMutableDictionary *paint = layer[@"paint"];
            paint[MLNFillExtrusionShadowProperty] = @NO;
        }
    }
    NSURL *target = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"liberty-shadow-runtime.json"]];
    [[NSJSONSerialization dataWithJSONObject:style options:0 error:nil] writeToURL:target atomically:YES];
    return target;
}

- (NSURL *)modelStyleURL {
    NSURL *styleURL = [[NSBundle mainBundle] URLForResource:@"positron-gltf" withExtension:@"json"];
    NSAssert(styleURL, @"The bundled Positron GLTF style is missing");
    return styleURL;
}

- (NSURL *)rectangleStyleURL {
    NSURL *styleURL = [[NSBundle mainBundle] URLForResource:@"rectangle-style" withExtension:@"json"];
    NSAssert(styleURL, @"The bundled rectangle style is missing");
    return styleURL;
}

- (void)updateShadowToggleTitle {
    NSString *title = self.shadowEnabled ? @"Shadows: ON · tap to compare" : @"Shadows: OFF · tap to compare";
    [self.shadowToggle setTitle:title forState:UIControlStateNormal];
    self.shadowToggle.backgroundColor = self.shadowEnabled
                                            ? [UIColor colorWithRed:0.08 green:0.45 blue:0.22 alpha:0.92]
                                            : [UIColor colorWithRed:0.55 green:0.12 blue:0.12 alpha:0.92];
}

- (void)toggleShadows {
    if (self.showingModel || self.showingRectangles) return;
    self.shadowEnabled = !self.shadowEnabled;
    MLNMapCamera *camera = self.mapView.camera;
    self.mapView.styleURL = [self shadowStyleURLWithShadowEnabled:self.shadowEnabled];
    self.mapView.camera = camera;
    [self updateShadowToggleTitle];
}

- (void)updateSceneControls {
    [self.sceneToggle setTitle:self.showingModel ? @"Show building shadows"
                              : self.showingRectangles ? @"Show Eiffel Tower GLB" : @"Show rectangle layer"
                      forState:UIControlStateNormal];
    self.sceneToggle.frame = CGRectMake(18, (self.showingModel || self.showingRectangles) ? 66 : 116, 252, 42);
    self.shadowToggle.hidden = self.showingModel || self.showingRectangles;
}

- (void)toggleScene {
    if (self.showingModel) {
        self.showingModel = NO;
    } else if (!self.showingRectangles) {
        self.showingRectangles = YES;
    } else {
        self.showingRectangles = NO;
        self.showingModel = YES;
    }
    self.mapView.styleURL = self.showingModel ? [self modelStyleURL]
                            : self.showingRectangles ? [self rectangleStyleURL]
                                                     : [self shadowStyleURLWithShadowEnabled:self.shadowEnabled];
    CLLocationCoordinate2D center = self.showingModel ? CLLocationCoordinate2DMake(48.8582621, 2.2944962)
                                    : self.showingRectangles ? CLLocationCoordinate2DMake(48.8566, 2.3522)
                                                             : CLLocationCoordinate2DMake(52.5096, 13.3760);
    self.mapView.camera = [MLNMapCamera cameraLookingAtCenterCoordinate:center
                                                               altitude:self.showingModel ? 650 : self.showingRectangles ? 1200000 : 850
                                                                  pitch:self.showingModel ? 62 : self.showingRectangles ? 0 : 52
                                                                heading:self.showingModel ? 28 : self.showingRectangles ? 0 : 20];
    [self updateSceneControls];
}

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    NSError *registrationError = nil;
    if (![MLNFillExtrusionShadowsPlugin registerPluginWithError:&registrationError]) {
        NSLog(@"Unable to register fill-extrusion shadows: %@", registrationError);
        return NO;
    }
    if (![MLNGltfLayerPlugin registerPluginWithError:&registrationError]) {
        NSLog(@"Unable to register GLTF layer: %@", registrationError);
        return NO;
    }
    if (![MLNRectangleLayerPlugin registerPluginWithError:&registrationError]) {
        NSLog(@"Unable to register rectangle layer: %@", registrationError);
        return NO;
    }

    self.shadowEnabled = ![NSProcessInfo.processInfo.environment[@"SHADOW_ENABLED"] isEqualToString:@"0"];
    NSString *initialScene = NSProcessInfo.processInfo.environment[@"PLUGIN_SCENE"] ?: @"model";
    self.showingRectangles = [initialScene isEqualToString:@"rectangle"];
    self.showingModel = !self.showingRectangles && ![initialScene isEqualToString:@"shadows"];
    NSURL *styleURL = self.showingModel ? [self modelStyleURL]
                       : self.showingRectangles ? [self rectangleStyleURL]
                                                : [self shadowStyleURLWithShadowEnabled:self.shadowEnabled];

    self.window = [[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
    UIViewController *controller = [[UIViewController alloc] init];
    self.mapView = [[MLNMapView alloc] initWithFrame:controller.view.bounds styleURL:styleURL];
    self.mapView.delegate = self;
    self.mapView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    CLLocationCoordinate2D initialCenter = self.showingModel ? CLLocationCoordinate2DMake(48.8582621, 2.2944962)
                                             : self.showingRectangles ? CLLocationCoordinate2DMake(48.8566, 2.3522)
                                                                      : CLLocationCoordinate2DMake(52.5096, 13.3760);
    self.mapView.camera = [MLNMapCamera cameraLookingAtCenterCoordinate:initialCenter
                                                               altitude:self.showingModel ? 650 : self.showingRectangles ? 1200000 : 850
                                                                  pitch:self.showingModel ? 62 : self.showingRectangles ? 0 : 52
                                                                heading:self.showingModel ? 28 : self.showingRectangles ? 0 : 20];
    [controller.view addSubview:self.mapView];

    self.shadowToggle = [UIButton buttonWithType:UIButtonTypeSystem];
    self.shadowToggle.frame = CGRectMake(18, 66, 252, 42);
    self.shadowToggle.autoresizingMask = UIViewAutoresizingFlexibleRightMargin | UIViewAutoresizingFlexibleBottomMargin;
    self.shadowToggle.layer.cornerRadius = 10;
    self.shadowToggle.titleLabel.font = [UIFont boldSystemFontOfSize:15];
    [self.shadowToggle setTitleColor:UIColor.whiteColor forState:UIControlStateNormal];
    [self.shadowToggle addTarget:self action:@selector(toggleShadows) forControlEvents:UIControlEventTouchUpInside];
    [self updateShadowToggleTitle];
    [controller.view addSubview:self.shadowToggle];

    self.sceneToggle = [UIButton buttonWithType:UIButtonTypeSystem];
    self.sceneToggle.frame = CGRectMake(18, 66, 252, 42);
    self.sceneToggle.backgroundColor = [UIColor colorWithWhite:0.1 alpha:0.9];
    self.sceneToggle.layer.cornerRadius = 10;
    [self.sceneToggle setTitleColor:UIColor.whiteColor forState:UIControlStateNormal];
    [self.sceneToggle addTarget:self action:@selector(toggleScene) forControlEvents:UIControlEventTouchUpInside];
    [controller.view addSubview:self.sceneToggle];
    [self updateSceneControls];
    self.window.rootViewController = controller;
    [self.window makeKeyAndVisible];
    return YES;
}

@end
