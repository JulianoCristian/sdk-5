// Copyright (c) 2015, the Fletch project authors. Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE.md file.

#define TESTING

#include "src/shared/assert.h"
#include "person_shared.h"
#include "cc/person_counter.h"

#include <cstdio>
#include <stdint.h>
#include <sys/time.h>

static void BuildPerson(PersonBuilder person, int n) {
  person.setAge(n * 20);
  if (n > 1) {
    List<PersonBuilder> children = person.initChildren(2);
    BuildPerson(children[0], n - 1);
    BuildPerson(children[1], n - 1);
  }
}

static int Depth(Node node) {
  if (node.isNum()) return 1;
  int left = Depth(node.getCons().getFst());
  int right = Depth(node.getCons().getSnd());
  return 1 + ((left > right) ? left : right);
}

static void FooCallback() {
}

static void PingCallback(int result) {
  EXPECT_EQ(42, result);
}

static void RunPersonTests() {
  MessageBuilder builder(512);

  PersonBuilder person = builder.initRoot<PersonBuilder>();
  BuildPerson(person, 7);
  EXPECT_EQ(3120, builder.ComputeUsed());

  int age = PersonCounter::getAge(person);
  EXPECT_EQ(140, age);
  int count = PersonCounter::count(person);
  EXPECT_EQ(127, count);

  AgeStats stats = PersonCounter::getAgeStats(person);
  EXPECT_EQ(39, stats.getAverageAge());
  EXPECT_EQ(4940, stats.getSum());
  stats.Delete();

  AgeStats stats2 = PersonCounter::createAgeStats(42, 42);
  EXPECT_EQ(42, stats2.getAverageAge());
  EXPECT_EQ(42, stats2.getSum());
  stats2.Delete();

  Person generated = PersonCounter::createPerson(10);
  EXPECT_EQ(42, generated.getAge());
  EXPECT_EQ(1, generated.getName().length());
  EXPECT_EQ(11, generated.getName()[0]);

  List<Person> children = generated.getChildren();
  EXPECT_EQ(10, children.length());
  for (int i = 0; i < children.length(); i++) {
    EXPECT_EQ(12 + i * 2, children[i].getAge());
  }
  generated.Delete();

  Node node = PersonCounter::createNode(10);
  EXPECT_EQ(24680, node.ComputeUsed());

  EXPECT_EQ(10, Depth(node));
  node.Delete();

  PersonCounter::foo();
  PersonCounter::fooAsync(FooCallback);

  EXPECT_EQ(42, PersonCounter::ping());
  PersonCounter::pingAsync(PingCallback);
}

static void RunPersonBoxTests() {
  MessageBuilder builder(512);

  PersonBoxBuilder box = builder.initRoot<PersonBoxBuilder>();
  PersonBuilder person = box.initPerson();
  person.setAge(87);
  List<uint8_t> name = person.initName(1);
  name[0] = 99;

  int age = PersonCounter::getBoxedAge(box);
  EXPECT_EQ(87, age);
}

static void BuildNode(NodeBuilder node, int n) {
  if (n > 1) {
    ConsBuilder cons = node.initCons();
    BuildNode(cons.initFst(), n - 1);
    BuildNode(cons.initSnd(), n - 1);
  } else {
    node.setCond(true);
    node.setNum(42);
  }
}

static void RunNodeTests() {
  MessageBuilder builder(512);

  NodeBuilder root = builder.initRoot<NodeBuilder>();
  BuildNode(root, 10);
  int depth = PersonCounter::depth(root);
  EXPECT_EQ(10, depth);
}

static void InteractWithService() {
  PersonCounter::setup();
  RunPersonTests();
  RunPersonBoxTests();
  RunNodeTests();
  PersonCounter::tearDown();
}

int main(int argc, char** argv) {
  if (argc < 2) {
    printf("Usage: %s <snapshot>\n", argv[0]);
    return 1;
  }
  SetupPersonTest(argc, argv);
  InteractWithService();
  TearDownPersonTest();
  return 0;
}
