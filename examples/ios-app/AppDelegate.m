#import "AppDelegate.h"

#import <FillExtrusionShadows/FillExtrusionShadows.h>
#import <GltfLayer/GltfLayer.h>
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
@end

@implementation AppDelegate

- (void)mapView:(MLNMapView *)mapView didFinishLoadingStyle:(MLNStyle *)style {
    MLNStyleLayer *modelLayer = [style layerWithIdentifier:@"eiffel-tower-gltf"];
    NSLog(@"Loaded style with %lu layers; GLTF Objective-C wrapper %@",
          (unsigned long)style.layers.count,
          modelLayer ? @"is present" : @"is missing");
    CLLocationCoordinate2D center = modelLayer
                                        ? CLLocationCoordinate2DMake(48.8582621, 2.2944962)
                                        : CLLocationCoordinate2DMake(52.5096, 13.3760);
    mapView.camera = [MLNMapCamera cameraLookingAtCenterCoordinate:center
                                                          altitude:modelLayer ? 650 : 850
                                                             pitch:modelLayer ? 62 : 52
                                                           heading:modelLayer ? 28 : 20];
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

- (void)updateShadowToggleTitle {
    NSString *title = self.shadowEnabled ? @"Shadows: ON · tap to compare" : @"Shadows: OFF · tap to compare";
    [self.shadowToggle setTitle:title forState:UIControlStateNormal];
    self.shadowToggle.backgroundColor = self.shadowEnabled
                                            ? [UIColor colorWithRed:0.08 green:0.45 blue:0.22 alpha:0.92]
                                            : [UIColor colorWithRed:0.55 green:0.12 blue:0.12 alpha:0.92];
}

- (void)toggleShadows {
    if (self.showingModel) return;
    self.shadowEnabled = !self.shadowEnabled;
    MLNMapCamera *camera = self.mapView.camera;
    self.mapView.styleURL = [self shadowStyleURLWithShadowEnabled:self.shadowEnabled];
    self.mapView.camera = camera;
    [self updateShadowToggleTitle];
}

- (void)updateSceneControls {
    [self.sceneToggle setTitle:self.showingModel ? @"Show building shadows" : @"Show Eiffel Tower GLB"
                      forState:UIControlStateNormal];
    self.sceneToggle.frame = CGRectMake(18, self.showingModel ? 66 : 116, 252, 42);
    self.shadowToggle.hidden = self.showingModel;
}

- (void)toggleScene {
    self.showingModel = !self.showingModel;
    self.mapView.styleURL = self.showingModel
                                ? [self modelStyleURL]
                                : [self shadowStyleURLWithShadowEnabled:self.shadowEnabled];
    CLLocationCoordinate2D center = self.showingModel
                                        ? CLLocationCoordinate2DMake(48.8582621, 2.2944962)
                                        : CLLocationCoordinate2DMake(52.5096, 13.3760);
    self.mapView.camera = [MLNMapCamera cameraLookingAtCenterCoordinate:center
                                                               altitude:self.showingModel ? 650 : 850
                                                                  pitch:self.showingModel ? 62 : 52
                                                                heading:self.showingModel ? 28 : 20];
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

    self.shadowEnabled = ![NSProcessInfo.processInfo.environment[@"SHADOW_ENABLED"] isEqualToString:@"0"];
    self.showingModel = YES;
    NSURL *styleURL = [self modelStyleURL];

    self.window = [[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
    UIViewController *controller = [[UIViewController alloc] init];
    self.mapView = [[MLNMapView alloc] initWithFrame:controller.view.bounds styleURL:styleURL];
    self.mapView.delegate = self;
    self.mapView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    CLLocationCoordinate2D initialCenter = self.showingModel
                                               ? CLLocationCoordinate2DMake(48.8582621, 2.2944962)
                                               : CLLocationCoordinate2DMake(52.5096, 13.3760);
    self.mapView.camera = [MLNMapCamera cameraLookingAtCenterCoordinate:initialCenter
                                                               altitude:self.showingModel ? 650 : 850
                                                                  pitch:self.showingModel ? 62 : 52
                                                                heading:self.showingModel ? 28 : 20];
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
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 8 * NSEC_PER_SEC), dispatch_get_main_queue(), ^{
      NSLog(@"FillExtrusionShadows render callback count: %llu",
            [MLNFillExtrusionShadowsPlugin renderCallbackCount]);
      NSLog(@"GLTF callbacks prepare=%llu load=%llu render=%llu vertices=%llu zoom=%.2f",
            [MLNGltfLayerPlugin prepareCallbackCount],
            [MLNGltfLayerPlugin loadCallbackCount],
            [MLNGltfLayerPlugin renderCallbackCount],
            [MLNGltfLayerPlugin loadedVertexCount],
            self.mapView.zoomLevel);
    });
    return YES;
}

@end
