#import "AppDelegate.h"

#import <FillExtrusionShadows/FillExtrusionShadows.h>
#import "MLNMapCamera.h"
#import "MLNMapView.h"

@interface AppDelegate ()
@property(nonatomic, strong) MLNMapView *mapView;
@property(nonatomic, strong) UIButton *shadowToggle;
@property(nonatomic) BOOL shadowEnabled;
@end

@implementation AppDelegate

- (NSURL *)styleURLWithShadowEnabled:(BOOL)enabled {
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

- (void)updateShadowToggleTitle {
    NSString *title = self.shadowEnabled ? @"Shadows: ON · tap to compare" : @"Shadows: OFF · tap to compare";
    [self.shadowToggle setTitle:title forState:UIControlStateNormal];
    self.shadowToggle.backgroundColor = self.shadowEnabled
                                            ? [UIColor colorWithRed:0.08 green:0.45 blue:0.22 alpha:0.92]
                                            : [UIColor colorWithRed:0.55 green:0.12 blue:0.12 alpha:0.92];
}

- (void)toggleShadows {
    self.shadowEnabled = !self.shadowEnabled;
    MLNMapCamera *camera = self.mapView.camera;
    self.mapView.styleURL = [self styleURLWithShadowEnabled:self.shadowEnabled];
    self.mapView.camera = camera;
    [self updateShadowToggleTitle];
}

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    NSError *registrationError = nil;
    if (![MLNFillExtrusionShadowsPlugin registerPluginWithError:&registrationError]) {
        NSLog(@"Unable to register fill-extrusion shadows: %@", registrationError);
        return NO;
    }

    self.shadowEnabled = ![NSProcessInfo.processInfo.environment[@"SHADOW_ENABLED"] isEqualToString:@"0"];
    NSURL *styleURL = [self styleURLWithShadowEnabled:self.shadowEnabled];

    self.window = [[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
    UIViewController *controller = [[UIViewController alloc] init];
    self.mapView = [[MLNMapView alloc] initWithFrame:controller.view.bounds styleURL:styleURL];
    self.mapView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    self.mapView.camera = [MLNMapCamera cameraLookingAtCenterCoordinate:CLLocationCoordinate2DMake(52.5096, 13.3760)
                                                               altitude:850
                                                                  pitch:52
                                                                heading:20];
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
    self.window.rootViewController = controller;
    [self.window makeKeyAndVisible];
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 8 * NSEC_PER_SEC), dispatch_get_main_queue(), ^{
      NSLog(@"FillExtrusionShadows render callback count: %llu",
            [MLNFillExtrusionShadowsPlugin renderCallbackCount]);
    });
    return YES;
}

@end
