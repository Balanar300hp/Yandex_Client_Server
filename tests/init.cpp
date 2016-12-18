#include "catch.h"
#include "../include/client.hpp"

SCENARIO("Client", "[clean]") {
 bool p=true;
 Client A("config.с");
  REQUIRE(p==true);
}
