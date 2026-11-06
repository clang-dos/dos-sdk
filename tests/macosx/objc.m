/* Objective-C on PowerPC Mac OS X with the fragile (Objective-C 1) runtime
 * and Foundation: classes, instance variables, categories, protocols,
 * super calls, struct and float returns, introspection, string literals,
 * collections, NS_DURING exceptions and (from 10.3) @try.
 *
 * Link with -framework Foundation -lobjc. 10.0's libraries use a flat
 * namespace, so libobjc must be named even though Foundation uses it. */
#import <Foundation/Foundation.h>
#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c)                                                               \
  do {                                                                         \
    if (!(c)) {                                                                \
      printf("FAIL line %d: %s\n", __LINE__, #c);                              \
      fails++;                                                                 \
    }                                                                          \
  } while (0)

typedef struct { int x, y, w, h; } Box;

@protocol Shape
- (double)area;
@end

@interface Base : NSObject {
  int count;
}
- (int)bump;
@end

@implementation Base
- (int)bump { return ++count; }
@end

@interface Rectangle : Base <Shape> {
  double width, height;
}
- (id)initWithWidth:(double)w height:(double)h;
- (Box)box;
@end

@implementation Rectangle
- (id)initWithWidth:(double)w height:(double)h {
  if ((self = [super init]) != nil) {
    width = w;
    height = h;
  }
  return self;
}
- (int)bump { return [super bump] * 10; }
- (double)area { return width * height; }
- (Box)box {
  Box b = {1, 2, (int)width, (int)height};
  return b; /* returned through a hidden pointer: objc_msgSend_stret */
}
@end

@interface Rectangle (Describe)
- (NSString *)describe;
@end

@implementation Rectangle (Describe)
- (NSString *)describe {
  return [NSString stringWithFormat:@"%gx%g", width, height];
}
@end

int main(void) {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  Rectangle *r = [[Rectangle alloc] initWithWidth:3 height:4.5];
  Box b;
  NSMutableArray *list;
  NSDictionary *d;
  volatile int caught = 0; /* set in a handler, after a longjmp */

  CHECK([r area] == 13.5);
  CHECK([r bump] == 10 && [r bump] == 20);
  b = [r box];
  CHECK(b.x == 1 && b.y == 2 && b.w == 3 && b.h == 4);
  CHECK([[r describe] isEqualToString:@"3x4.5"]);
  CHECK([r isKindOfClass:[Base class]] && ![r isMemberOfClass:[Base class]]);
  CHECK([r conformsToProtocol:@protocol(Shape)]);
  CHECK([r respondsToSelector:@selector(describe)]);
  CHECK(![r respondsToSelector:@selector(noSuchMethod)]);
  CHECK(strcmp([NSStringFromClass([r class]) cString], "Rectangle") == 0);

  list = [NSMutableArray array];
  [list addObject:@"one"];
  [list addObject:[NSNumber numberWithInt:2]];
  [list addObject:r];
  CHECK([list count] == 3);
  CHECK([[list objectAtIndex:1] intValue] == 2);
  d = [NSDictionary dictionaryWithObject:@"v" forKey:@"k"];
  CHECK([[d objectForKey:@"k"] isEqualToString:@"v"]);
  CHECK([@"abc" length] == 3);

  NS_DURING
    [NSException raise:NSInvalidArgumentException format:@"bad %d", 7];
  NS_HANDLER
    caught = [[localException reason] isEqualToString:@"bad 7"];
  NS_ENDHANDLER
  CHECK(caught);

#if __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ >= 1030
  /* @try needs objc_exception_try_enter, which libobjc has from 10.3. */
  caught = 0;
  @try {
    [list objectAtIndex:99];
  } @catch (NSException *e) {
    caught = [[e name] isEqualToString:NSRangeException];
  } @finally {
    caught *= 2;
  }
  CHECK(caught == 2);
#endif

  [r release];
  [pool release];
  printf(fails ? "%d FAILED\n" : "ALL PASSED\n", fails);
  return fails != 0;
}
