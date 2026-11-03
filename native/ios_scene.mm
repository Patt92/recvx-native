// UIScene-Lebenszyklus fuer iOS/tvOS. Plattformanpassung, keine Spiellogik.
// Mit dem iOS-27-SDK gelinkte Apps ohne UIApplicationSceneManifest beendet UIKit beim Start
// (_UIApplicationEvaluateRuntimeIssueForNoSceneLifecycleAdoption). SDL 2.32 kennt keine Scenes und legt sein
// UIWindow ohne windowScene an; RECVXSceneDelegate merkt sich die Scene, recvx_ios_adopt_scene haengt das
// SDL-Fenster daran und zeigt es.
#import <UIKit/UIKit.h>
#include <SDL.h>
#include <SDL_syswm.h>
#include <stdexcept>
#include "ios_scene.h"

static UIWindowScene* recvx_window_scene=nil;

@interface RECVXSceneDelegate:UIResponder<UIWindowSceneDelegate>
@property(strong,nonatomic) UIWindow* window;
@end
@implementation RECVXSceneDelegate
-(void)scene:(UIScene*)scene willConnectToSession:(UISceneSession*)session options:(UISceneConnectionOptions*)options{
    if([scene isKindOfClass:[UIWindowScene class]])recvx_window_scene=(UIWindowScene*)scene;
}
-(void)sceneDidDisconnect:(UIScene*)scene{if(scene==recvx_window_scene)recvx_window_scene=nil;}
@end

namespace recvx {
void ios_adopt_scene(SDL_Window* window){
    SDL_SysWMinfo info;SDL_VERSION(&info.version);
    if(!SDL_GetWindowWMInfo(window,&info)||info.subsystem!=SDL_SYSWM_UIKIT)throw std::runtime_error("UIKit window unavailable");
    // Die Scene verbindet UIKit im Runloop nach didFinishLaunching; SDL ruft die Runtime ebenfalls von dort auf.
    for(int i=0;i<1000&&!recvx_window_scene;++i)CFRunLoopRunInMode(kCFRunLoopDefaultMode,0.01,true);
    if(!recvx_window_scene)throw std::runtime_error("no UIWindowScene connected");
    UIWindow* uiwindow=info.info.uikit.window;
    uiwindow.windowScene=recvx_window_scene;
    [uiwindow makeKeyAndVisible];
}
bool ios_fit_view(SDL_Window* window){
    SDL_SysWMinfo info;SDL_VERSION(&info.version);
    if(!SDL_GetWindowWMInfo(window,&info)||info.subsystem!=SDL_SYSWM_UIKIT)return false;
    UIWindow* uiwindow=info.info.uikit.window;UIView* view=uiwindow.rootViewController.view;
    if(!view||CGRectEqualToRect(view.frame,uiwindow.bounds))return false;
    view.frame=uiwindow.bounds;[view setNeedsLayout];[view layoutIfNeeded]; // layoutSubviews setzt drawableSize neu
    return true;
}
bool ios_exclude_from_backup(const char* path,std::string& reason){
    NSURL* url=[NSURL fileURLWithPath:[NSString stringWithUTF8String:path] isDirectory:YES];
    NSError* error=nil;
    if([url setResourceValue:@YES forKey:NSURLIsExcludedFromBackupKey error:&error])return true;
    reason=error?error.localizedDescription.UTF8String:"NSURLIsExcludedFromBackupKey not set";
    return false;
}
}
