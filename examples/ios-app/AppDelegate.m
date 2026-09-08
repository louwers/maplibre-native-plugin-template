#import "AppDelegate.h"

#import <GltfLayer/GltfLayer.h>
#import <RectangleLayer/RectangleLayer.h>
#import "MLNMapCamera.h"
#import "MLNMapView.h"
#import "MLNMapViewDelegate.h"
#import "MLNStyle.h"

@interface AppDelegate () <MLNMapViewDelegate>
@property(nonatomic, strong) MLNMapView *mapView;
@property(nonatomic, strong) UISegmentedControl *scenes;
@end

@implementation AppDelegate

- (NSURL *)selectedStyleURL {
    NSString *name = self.scenes.selectedSegmentIndex == 0 ? @"positron-gltf" : @"rectangle-style";
    NSURL *url = [[NSBundle mainBundle] URLForResource:name withExtension:@"json"];
    NSAssert(url, @"The bundled plugin style is missing");
    return url;
}

- (void)mapView:(MLNMapView *)mapView didFinishLoadingStyle:(MLNStyle *)style {
    BOOL model = [style layerWithIdentifier:@"eiffel-tower-gltf"] != nil;
    CLLocationCoordinate2D center = model ? CLLocationCoordinate2DMake(48.8582621, 2.2944962)
                                          : CLLocationCoordinate2DMake(48.8566, 2.3522);
    mapView.camera = [MLNMapCamera cameraLookingAtCenterCoordinate:center
                                                        altitude:model ? 650 : 1200000
                                                           pitch:model ? 62 : 0
                                                         heading:model ? 28 : 0];
}

- (void)selectScene {
    self.mapView.styleURL = [self selectedStyleURL];
}

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    NSError *error = nil;
    if (![MLNGltfLayerPlugin registerPluginWithError:&error] ||
        ![MLNRectangleLayerPlugin registerPluginWithError:&error]) {
        NSLog(@"Unable to register plugin: %@", error);
        return NO;
    }

    self.window = [[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
    UIViewController *controller = [[UIViewController alloc] init];
    self.scenes = [[UISegmentedControl alloc] initWithItems:@[@"Eiffel Tower", @"Rectangles"]];
    self.scenes.selectedSegmentIndex =
        [NSProcessInfo.processInfo.environment[@"PLUGIN_SCENE"] isEqualToString:@"rectangle"] ? 1 : 0;
    [self.scenes addTarget:self action:@selector(selectScene) forControlEvents:UIControlEventValueChanged];

    self.mapView = [[MLNMapView alloc] initWithFrame:controller.view.bounds styleURL:[self selectedStyleURL]];
    self.mapView.delegate = self;
    self.mapView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    [controller.view addSubview:self.mapView];

    self.scenes.translatesAutoresizingMaskIntoConstraints = NO;
    self.scenes.backgroundColor = UIColor.systemBackgroundColor;
    [controller.view addSubview:self.scenes];
    [NSLayoutConstraint activateConstraints:@[
        [self.scenes.topAnchor constraintEqualToAnchor:controller.view.safeAreaLayoutGuide.topAnchor constant:12],
        [self.scenes.centerXAnchor constraintEqualToAnchor:controller.view.centerXAnchor],
    ]];
    self.window.rootViewController = controller;
    [self.window makeKeyAndVisible];
    return YES;
}

@end
