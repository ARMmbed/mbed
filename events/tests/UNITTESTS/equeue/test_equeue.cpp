/*
 * Copyright (c) 2019, Arm Limited and affiliates.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "gtest/gtest.h"
#include "events/equeue.h"
#include "platform/Callback.h"
#include <unistd.h>
#include <pthread.h>

#define EVENTS_EVENT_SIZE (EQUEUE_EVENT_SIZE - 2*sizeof(void*) + sizeof(mbed::Callback<void()>))
#define TEST_EQUEUE_SIZE 2048
#define TEST_THREAD_STACK_SIZE 512
#define DISPATCH_INFINITE -1
#define ITERATION_TIMES 10

extern unsigned int equeue_global_time;

class TestEqueue : public testing::Test {
    virtual void SetUp()
    {
    }

    virtual void TearDown()
    {
    }
};

// Test functions
static void pass_func(void *eh)
{
}

static void simple_func(void *p)
{
    uint8_t *d = reinterpret_cast<uint8_t *>(p);
    if (*d < 255) {
        (*d)++;
    }
}

static void sloth_func(void *p)
{
    // adding to equeue_global_time becouse this simulates that this function takes some time
    equeue_global_time += 10;
    (*(reinterpret_cast<uint8_t *>(p)))++;
}

struct indirect {
    uint8_t *touched;
    uint8_t buffer[7];
};

static void indirect_func(void *p)
{
    struct indirect *i = reinterpret_cast<struct indirect *>(p);
    (*i->touched)++;
}

struct timing {
    unsigned tick;
    unsigned delay;
};

static void timing_func(void *p)
{
    struct timing *timing = reinterpret_cast<struct timing *>(p);
    unsigned tick = equeue_tick();

    unsigned t1 = timing->delay;
    unsigned t2 = tick - timing->tick;
    EXPECT_TRUE(t2 - 10 < t1 < t2 + 10);

    timing->tick = tick;
}

struct fragment {
    equeue_t *q;
    size_t size;
    struct timing timing;
};

static void fragment_func(void *p)
{
    struct fragment *fragment = reinterpret_cast<struct fragment *>(p);
    timing_func(&fragment->timing);

    struct fragment *nfragment = reinterpret_cast<struct fragment *>(equeue_alloc(fragment->q, fragment->size));
    ASSERT_TRUE(nfragment != NULL);

    *nfragment = *fragment;
    equeue_event_delay(nfragment, fragment->timing.delay);

    int id = equeue_post(nfragment->q, fragment_func, nfragment);
    ASSERT_NE(0, id);
}

struct cancel {
    equeue_t *q;
    int id;
};

static void cancel_func(void *p)
{
    struct cancel *ccel = reinterpret_cast<struct cancel *>(p);
    equeue_cancel(ccel->q, ccel->id);
}

struct nest {
    equeue_t *q;
    void (*cb)(void *);
    void *data;
};

static void nest_func(void *p)
{
    struct nest *nst = reinterpret_cast<struct nest *>(p);
    equeue_call(nst->q, nst->cb, nst->data);
    // adding to equeue_global_time becouse this simulates that this function takes some time
    equeue_global_time += 10;
}

static void *multithread_thread(void *p)
{
    equeue_t *q = reinterpret_cast<equeue_t *>(p);
    equeue_dispatch(q, DISPATCH_INFINITE);
    return 0;
}

class ecount {
    mutable pthread_mutex_t mutex;
    pthread_cond_t cond;
    uint8_t count;
public:
    ecount() : count(0)
    {
        int err = pthread_mutex_init(&mutex, NULL);
        EXPECT_EQ(0, err);
        err = pthread_cond_init(&cond, NULL);
        EXPECT_EQ(0, err);
    }

    ~ecount()
    {
        int err = pthread_mutex_destroy(&mutex);
        EXPECT_EQ(0, err);
        err = pthread_cond_destroy(&cond);
        EXPECT_EQ(0, err);
    }

    void lock() const
    {
        int err = pthread_mutex_lock(&mutex);
        EXPECT_EQ(0, err);
    }

    void unlock() const
    {
        int err = pthread_mutex_unlock(&mutex);
        EXPECT_EQ(0, err);
    }

    void touch()
    {
        lock();
        if (count < 200) {
            count++;
        }
        unlock();
        int err = pthread_cond_broadcast(&cond);
        EXPECT_EQ(0, err);
    }

    void wait_for_touches(uint8_t n)
    {
        lock();
        while (count < n) {
            int err = pthread_cond_wait(&cond, &mutex);
            EXPECT_EQ(0, err);
        }
        unlock();
    }
};

static void multithread_func(void *p)
{
    static_cast<ecount *>(p)->touch();
}

static void background_func(void *p, int ms)
{
    *(reinterpret_cast<int *>(p)) = ms;
}

struct ethread {
    pthread_t thread;
    equeue_t *q;
    int ms;
};

static void *ethread_dispatch(void *p)
{
    struct ethread *t = reinterpret_cast<struct ethread *>(p);
    equeue_dispatch(t->q, t->ms);
    return 0;
}

struct count_and_queue {
    int p;
    equeue_t *q;
};

static void simple_breaker(void *p)
{
    struct count_and_queue *caq = reinterpret_cast<struct count_and_queue *>(p);
    equeue_break(caq->q);
    // adding to equeue_global_time becouse this simulates that this function takes some time
    equeue_global_time += 10;
    caq->p++;
}

// Simple call tests

/** Test that equeue executes function passed by equeue_call.
 *
 *  Given queue is initialized.
 *  When the event is scheduled and after that equeue_dispatch is called.
 *  Then function passed by equeue_call is executed properly.
 */
TEST_F(TestEqueue, test_equeue_simple_call)
{
    equeue_t q;
    int err = equeue_create(&q, TEST_EQUEUE_SIZE);
    ASSERT_EQ(0, err);

    uint8_t touched = 0;
    equeue_call(&q, simple_func, &touched);
    equeue_dispatch(&q, 0);
    EXPECT_EQ(1, touched);

    touched = 0;
    equeue_dispatch(&q, 10);
    EXPECT_EQ(0, touched);

    equeue_destroy(&q);
}

/** Test that equeue executes function passed by equeue_call_in.
 *
 *  Given queue is initialized.
 *  When the event is scheduled and after that equeue_dispatch is called.
 *  Then function passed by equeue_call_in is executed properly.
 */
TEST_F(TestEqueue, test_equeue_simple_call_in)
{
    equeue_t q;
    int err = equeue_create(&q, TEST_EQUEUE_SIZE);
    ASSERT_EQ(0, err);

    uint8_t touched = 0;
    int id = equeue_call_in(&q, 10, simple_func, &touched);
    ASSERT_NE(0, id);

    equeue_dispatch(&q, 15);
    EXPECT_EQ(1, touched);

    touched = 0;
    equeue_dispatch(&q, 10);
    EXPECT_EQ(0, touched);

    equeue_destroy(&q);
}

/** Test that equeue executes function passed by equeue_call_every.
 *
 *  Given queue is initialized.
 *  When the event is scheduled and after that equeue_dispatch is called.
 *  Then function passed by equeue_call_every is executed properly.
 */

TEST_F(TestEqueue, test_equeue_simple_call_every)
{
    equeue_t q;
    int err = equeue_create(&q, TEST_EQUEUE_SIZE);
    ASSERT_EQ(0, err);

    uint8_t touched = 0;
    int id = equeue_call_every(&q, 10, simple_func, &touched);
    ASSERT_NE(0, id);

    equeue_dispatch(&q, 15);
    EXPECT_EQ(1, touched);

    equeue_destroy(&q);
}

/** Test that equeue executes function passed by equeue_post.
 *
 *  Given queue is initialized.
 *  When the event is posted and after that equeue_dispatch is called.
 *  Then function passed by equeue_post is executed properly.
 */
TEST_F(TestEqueue, test_equeue_simple_post)
{
    equeue_t q;
    int err = equeue_create(&q, TEST_EQUEUE_SIZE);
    ASSERT_EQ(0, err);

    uint8_t touched = 0;
    struct indirect *i = reinterpret_cast<struct indirect *>(equeue_alloc(&q, sizeof(struct indirect)));
    ASSERT_TRUE(i != NULL);

    i->touched = &touched;
    int id = equeue_post(&q, indirect_func, i);
    ASSERT_NE(0, id);

    equeue_dispatch(&q, 0);
    EXPECT_EQ(1, *i->touched);

    equeue_destroy(&q);
}

// Misc tests

/** Test that equeue executes events attached to its events destructors by equeue_event_dtor.
 *
 *  Given queue is initialized.
 *  When equeue events are being destroyed by equeue_dispatch, equeue_cancel, or equeue_destroy.
 *  Then functions attached to equeue events destructors are executed properly.
 */
TEST_F(TestEqueue, test_equeue_destructor)
{
    equeue_t q;
    int err = equeue_create(&q, TEST_EQUEUE_SIZE);
    ASSERT_EQ(0, err);

    uint8_t touched = 0;
    struct indirect *e;
    int ids[3];

    for (int i = 0; i < 3; i++) {
        e = reinterpret_cast<struct indirect *>(equeue_alloc(&q, sizeof(struct indirect)));
        ASSERT_TRUE(e != NULL);

        e->touched = &touched;
        equeue_event_dtor(e, indirect_func);
        int id = equeue_post(&q, pass_func, e);
        ASSERT_NE(0, id);
    }

    equeue_dispatch(&q, 0);
    EXPECT_EQ(3, touched);

    touched = 0;
    for (int i = 0; i < 3; i++) {
        e = reinterpret_cast<struct indirect *>(equeue_alloc(&q, sizeof(struct indirect)));
        ASSERT_TRUE(e != NULL);

        e->touched = &touched;
        equeue_event_dtor(e, indirect_func);
        ids[i] = equeue_post(&q, pass_func, e);
        ASSERT_NE(0, ids[i]);
    }

    for (int i = 0; i < 3; i++) {
        equeue_cancel(&q, ids[i]);
    }
    EXPECT_EQ(3, touched);

    equeue_dispatch(&q, 0);

    touched = 0;
    for (int i = 0; i < 3; i++) {
        e = reinterpret_cast<struct indirect *>(equeue_alloc(&q, sizeof(struct indirect)));
        ASSERT_TRUE(e);

        e->touched = &touched;
        equeue_event_dtor(e, indirect_func);
        int id = equeue_post(&q, pass_func, e);
        ASSERT_NE(0, id);
    }

    equeue_destroy(&q);
    EXPECT_EQ(3, touched);
}

/** Test that equeue_alloc returns 0 when equeue can not be allocated.
 *
 *  Given queue is initialized.
 *  When equeue_alloc is called and equeue can not be allocated
 *  Then function equeue_alloc returns NULL.
 */
TEST_F(TestEqueue, test_equeue_allocation_failure)
{
    equeue_t q;
    int err = equeue_create(&q, TEST_EQUEUE_SIZE);
    ASSERT_EQ(0, err);

    void *p = equeue_alloc(&q, 2 * TEST_EQUEUE_SIZE);
    EXPECT_TRUE(p == NULL);

    for (int i = 0; i < 100; i++) {
        p = equeue_alloc(&q, 0);
    }
    EXPECT_TRUE(p == NULL);

    equeue_destroy(&q);
}

/** Test that equeue does not execute evenets that has been canceled.
 *
 *  Given queue is initialized.
 *  When events are canceled by equeue_cancel.
 *  Then they are not executed by calling equeue_dispatch.
 */
TEST_F(TestEqueue, test_equeue_cancel)
{
    equeue_t q;
    int err = equeue_create(&q, (2 * ITERATION_TIMES * EVENTS_EVENT_SIZE));
    ASSERT_EQ(0, err);

    uint8_t touched = 0;
    int ids[2 * ITERATION_TIMES];

    for (int i = 0; i < 2 * ITERATION_TIMES; i++) {
        ids[i] = equeue_call(&q, simple_func, &touched);
        ASSERT_NE(0, ids[i]);
    }

    for (int i = 2 * ITERATION_TIMES - 1; i >= 0; i--) {
        equeue_cancel(&q, ids[i]);
    }

    equeue_dispatch(&q, 0);
    EXPECT_EQ(0, touched);

    equeue_destroy(&q);
}

/** Test that events can be cancelled by function executed by equeue_dispatch.
 *
 *  Given queue is initialized.
 *  When event is cancelled by another event while dispatching.
 *  Then event that was cancelled is not being executed.
 */
TEST_F(TestEqueue, test_equeue_cancel_inflight)
{
    equeue_t q;
    int err = equeue_create(&q, TEST_EQUEUE_SIZE);
    ASSERT_EQ(0, err);

    uint8_t touched = 0;

    int id = equeue_call(&q, simple_func, &touched);
    equeue_cancel(&q, id);

    equeue_dispatch(&q, 0);
    EXPECT_EQ(0, touched);

    id = equeue_call(&q, simple_func, &touched);
    equeue_cancel(&q, id);

    equeue_dispatch(&q, 0);
    EXPECT_EQ(0, touched);

    struct cancel *ccel = reinterpret_cast<struct cancel *>(equeue_alloc(&q, sizeof(struct cancel)));
    ASSERT_TRUE(ccel != NULL);
    ccel->q = &q;
    ccel->id = 0;

    id = equeue_post(&q, cancel_func, ccel);
    ASSERT_NE(0, id);

    ccel->id = equeue_call(&q, simple_func, &touched);

    equeue_dispatch(&q, 0);
    EXPECT_EQ(0, touched);

    equeue_destroy(&q);
}

/** Test that unnecessary canceling events would not affect executing other events.
 *
 *  Given queue is initialized.
 *  When event is unnecessary canceled by equeue_cancel.
 *  Then other events are properly executed after calling equeue_dispatch.
 */
TEST_F(TestEqueue, test_equeue_cancel_unnecessarily)
{
    equeue_t q;
    int err = equeue_create(&q, TEST_EQUEUE_SIZE);
    ASSERT_EQ(0, err);

    int id = equeue_call(&q, pass_func, 0);
    for (int i = 0; i < 5; i++) {
        equeue_cancel(&q, id);
    }

    id = equeue_call(&q, pass_func, 0);
    equeue_dispatch(&q, 0);
    for (int i = 0; i < 5; i++) {
        equeue_cancel(&q, id);
    }

    uint8_t touched = 0;
    equeue_call(&q, simple_func, &touched);
    for (int i = 0; i < 5; i++) {
        equeue_cancel(&q, id);
    }

    equeue_dispatch(&q, 0);
    EXPECT_EQ(1, touched);

    equeue_destroy(&q);
}

/** Test that dispatching events that have 0 ms period time would not end up in infinite loop.
 *
 *  Given queue is initialized.
 *  When events have 0 ms period time.
 *  Then dispatching would not end up in infinite loop.
 */
TEST_F(TestEqueue, test_equeue_loop_protect)
{
    equeue_t q;
    int err = equeue_create(&q, TEST_EQUEUE_SIZE);
    ASSERT_EQ(0, err);

    uint8_t touched1 = 0;
    equeue_call_every(&q, 0, simple_func, &touched1);

    equeue_dispatch(&q, 0);
    EXPECT_EQ(1, touched1);

    touched1 = 0;
    uint8_t touched2 = 0;
    equeue_call_every(&q, 1, simple_func, &touched2);

    equeue_dispatch(&q, 0);
    EXPECT_EQ(1, touched1);
    EXPECT_EQ(0, touched2);

    equeue_destroy(&q);
}

/** Test that equeue_break breaks event queue out of dispatching.
 *
 *  Given queue is initialized.
 *  When equeue_break is called.
 *  Then event queue will stop dispatching after finisching current dispatching cycle.
 */
TEST_F(TestEqueue, test_equeue_break)
{
    equeue_t q;
    int err = equeue_create(&q, TEST_EQUEUE_SIZE);
    ASSERT_EQ(0, err);

    uint8_t touched1 = 0;
    equeue_call_every(&q, 0, simple_func, &touched1);

    uint8_t touched2 = 0;
    equeue_call_every(&q, 5, simple_func, &touched2);

    equeue_break(&q);
    equeue_dispatch(&q, DISPATCH_INFINITE);
    EXPECT_EQ(1, touched1);
    EXPECT_EQ(0, touched2);

    equeue_destroy(&q);
}

/** Test that equeue_break function breaks equeue dispatching only once.
 *
 *  Given queue is initialized.
 *  When equeue_break is called several times.
 *  Then equeue is stopped only once.
 */
TEST_F(TestEqueue, test_equeue_break_no_windup)
{
    equeue_t q;
    int err = equeue_create(&q, TEST_EQUEUE_SIZE);
    ASSERT_EQ(0, err);

    uint8_t touched = 0;
    equeue_call_every(&q, 0, simple_func, &touched);

    equeue_break(&q);
    equeue_break(&q);
    equeue_dispatch(&q, DISPATCH_INFINITE);
    EXPECT_EQ(1, touched);

    touched = 0;
    equeue_dispatch(&q, 55);
    EXPECT_TRUE(touched > 1);

    equeue_destroy(&q);
}

/** Test that function passed by equeue_call_every is being executed periodically.
 *
 *  Given queue is initialized.
 *  When function is passed by equeue_call_every with specified period.
 *  Then event is executed (dispatch time/period) times.
 */

TEST_F(TestEqueue, test_equeue_period)
{
    equeue_t q;
    int err = equeue_create(&q, TEST_EQUEUE_SIZE);
    ASSERT_EQ(0, err);

    uint8_t touched = 0;
    equeue_call_every(&q, 10, simple_func, &touched);

    equeue_dispatch(&q, 55);
    EXPECT_EQ(5, touched);

    equeue_destroy(&q);
}

/** Test that function added to the equeue by other function which already is in equeue executes in the next dispatch, or after the end of execution of the "mother" event.
 *
 *  Given queue is initialized.
 *  When nested function is added to enqueue.
 *  Then it is executed in the next dispatch, or after execution of "mother" function.
 */
TEST_F(TestEqueue, test_equeue_nested)
{
    equeue_t q;
    int err = equeue_create(&q, TEST_EQUEUE_SIZE);
    ASSERT_EQ(0, err);

    uint8_t touched = 0;
    struct nest *nst = reinterpret_cast<struct nest *>(equeue_alloc(&q, sizeof(struct nest)));
    ASSERT_TRUE(nst != NULL);
    nst->q = &q;
    nst->cb = simple_func;
    nst->data = &touched;

    int id = equeue_post(&q, nest_func, nst);
    ASSERT_NE(0, id);

    equeue_dispatch(&q, 5);
    EXPECT_EQ(0, touched);

    equeue_dispatch(&q, 1);
    EXPECT_EQ(1, touched);

    touched = 0;
    nst = reinterpret_cast<struct nest *>(equeue_alloc(&q, sizeof(struct nest)));
    ASSERT_TRUE(nst != NULL);
    nst->q = &q;
    nst->cb = simple_func;
    nst->data = &touched;

    id = equeue_post(&q, nest_func, nst);
    ASSERT_NE(0, id);

    equeue_dispatch(&q, 20);
    EXPECT_EQ(1, touched);

    equeue_destroy(&q);
}

/** Test that functions scheduled after slow function would execute according to the schedule if it is possible, if not they would execute right after sloth function.
 *
 *  Given queue is initialized.
 *  When sloth function is being called before other functions.
 *  Then if it is possible all functions start according to predefined schedule correctly.
 */
TEST_F(TestEqueue, test_equeue_sloth)
{
    equeue_t q;
    int err = equeue_create(&q, TEST_EQUEUE_SIZE);
    ASSERT_EQ(0, err);

    uint8_t touched1 = 0;
    uint8_t touched2 = 0;
    uint8_t touched3 = 0;
    int id = equeue_call(&q, sloth_func, &touched1);
    ASSERT_NE(0, id);

    id = equeue_call_in(&q, 5, simple_func, &touched2);
    ASSERT_NE(0, id);

    id = equeue_call_in(&q, 15, simple_func, &touched3);
    ASSERT_NE(0, id);

    equeue_dispatch(&q, 20);
    EXPECT_EQ(1, touched1);
    EXPECT_EQ(1, touched2);
    EXPECT_EQ(1, touched3);

    equeue_destroy(&q);
}

/** Test that equeue can be broken of dispatching from a different thread.
 *
 *  Given queue is initialized.
 *  When equeue starts dispatching in one thread.
 *  Then it can be stopped from another thread via equeue_break.
 */
TEST_F(TestEqueue, test_equeue_multithread)
{
    equeue_t q;
    int err = equeue_create(&q, TEST_EQUEUE_SIZE);
    ASSERT_EQ(0, err);

    ecount t;
    equeue_call_every(&q, 1, multithread_func, &t);

    pthread_t thread;
    err = pthread_create(&thread, 0, multithread_thread, &q);
    ASSERT_EQ(0, err);

    t.wait_for_touches(1);
    equeue_break(&q);
    err = pthread_join(thread, 0);
    ASSERT_EQ(0, err);

    equeue_destroy(&q);
}

/** Test that variable referred via equeue_background shows value in ms to the next event.
 *
 *  Given queue is initialized.
 *  When variable is referred via equeue_background.
 *  Then it depicts the time in ms to the next event.
 */
TEST_F(TestEqueue, test_equeue_background)
{
    equeue_t q;
    int err = equeue_create(&q, TEST_EQUEUE_SIZE);
    ASSERT_EQ(0, err);

    int id = equeue_call_in(&q, 20, pass_func, 0);
    ASSERT_NE(0, id);

    int ms;
    equeue_background(&q, background_func, &ms);
    EXPECT_EQ(20, ms);

    id = equeue_call_in(&q, 10, pass_func, 0);
    ASSERT_NE(0, id);
    EXPECT_EQ(10, ms);

    id = equeue_call(&q, pass_func, 0);
    ASSERT_NE(0, id);
    EXPECT_EQ(0, ms);

    equeue_dispatch(&q, 0);
    EXPECT_EQ(10, ms);

    equeue_destroy(&q);
    EXPECT_EQ(-1, ms);
}

/** Test that when chaining two equeues, events from both equeues execute by calling dispatch only on target.
 *
 *  Given queue is initialized.
 *  When target chained equeue is dispatched.
 *  Then events from both chained equeues are executed.
 */
TEST_F(TestEqueue, test_equeue_chain)
{
    equeue_t q1;
    int err = equeue_create(&q1, TEST_EQUEUE_SIZE);
    ASSERT_EQ(0, err);

    equeue_t q2;
    err = equeue_create(&q2, TEST_EQUEUE_SIZE);
    ASSERT_EQ(0, err);

    equeue_chain(&q2, &q1);

    uint8_t touched1 = 0;
    uint8_t touched2 = 0;

    int id1 = equeue_call_in(&q1, 20, simple_func, &touched1);
    ASSERT_NE(0, id1);
    int id2 = equeue_call_in(&q2, 20, simple_func, &touched2);
    ASSERT_NE(0, id2);

    id1 = equeue_call(&q1, simple_func, &touched1);
    ASSERT_NE(0, id1);
    id2 = equeue_call(&q2, simple_func, &touched2);
    ASSERT_NE(0, id2);

    id1 = equeue_call_in(&q1, 5, simple_func, &touched1);
    ASSERT_NE(0, id1);
    id2 = equeue_call_in(&q2, 5, simple_func, &touched2);
    ASSERT_NE(0, id2);

    equeue_cancel(&q1, id1);
    equeue_cancel(&q2, id2);

    id1 = equeue_call_in(&q1, 10, simple_func, &touched1);
    ASSERT_NE(0, id1);
    id2 = equeue_call_in(&q2, 10, simple_func, &touched2);
    ASSERT_NE(0, id2);

    equeue_dispatch(&q1, 30);

    EXPECT_EQ(3, touched1);
    EXPECT_EQ(3, touched2);

    equeue_destroy(&q2);
    equeue_destroy(&q1);
}

/** Test that unchaining equeues makes them work on their own.
 *
 *  Given queue is initialized.
 *  When equeue is unchained.
 *  Then it can be only dispatched by calling with reference to it.
 */
TEST_F(TestEqueue, test_equeue_unchain)
{
    equeue_t q1;
    int err = equeue_create(&q1, TEST_EQUEUE_SIZE);
    ASSERT_EQ(0, err);

    equeue_t q2;
    err = equeue_create(&q2, TEST_EQUEUE_SIZE);
    ASSERT_EQ(0, err);

    equeue_chain(&q2, &q1);

    uint8_t touched1 = 0;
    uint8_t touched2 = 0;
    int id1 = equeue_call(&q1, simple_func, &touched1);
    ASSERT_NE(0, id1);
    int id2 = equeue_call(&q2, simple_func, &touched2);
    ASSERT_NE(0, id2);

    equeue_dispatch(&q1, 0);
    EXPECT_EQ(1, touched1);
    EXPECT_EQ(1, touched2);

    equeue_chain(&q2, 0);

    touched1 = 0;
    touched2 = 0;

    id1 = equeue_call(&q1, simple_func, &touched1);
    ASSERT_NE(0, id1);
    id2 = equeue_call(&q2, simple_func, &touched2);
    ASSERT_NE(0, id2);

    equeue_dispatch(&q1, 0);
    EXPECT_EQ(1, touched1);
    EXPECT_EQ(0, touched2);

    equeue_dispatch(&q2, 0);
    EXPECT_EQ(1, touched1);
    EXPECT_EQ(1, touched2);

    equeue_chain(&q1, &q2);

    touched1 = 0;
    touched2 = 0;

    id1 = equeue_call(&q1, simple_func, &touched1);
    ASSERT_NE(0, id1);
    id2 = equeue_call(&q2, simple_func, &touched2);
    ASSERT_NE(0, id2);

    equeue_dispatch(&q2, 0);
    EXPECT_EQ(1, touched1);
    EXPECT_EQ(1, touched2);

    equeue_destroy(&q1);
    equeue_destroy(&q2);
}

// Barrage tests

/** Test that equeue keeps good time at starting events.
 *
 *  Given queue is initialized.
 *  When equeue is being dispatched.
 *  Then events happen according to the schedule with an error within a specified range.
 */
TEST_F(TestEqueue, test_equeue_simple_barrage)
{
    equeue_t q;
    int err = equeue_create(&q, 2 * ITERATION_TIMES * (EQUEUE_EVENT_SIZE + sizeof(struct timing)));
    ASSERT_EQ(0, err);

    for (int i = 0; i < 2 * ITERATION_TIMES; i++) {
        struct timing *timing = reinterpret_cast<struct timing *>(equeue_alloc(&q, sizeof(struct timing)));
        ASSERT_TRUE(timing != NULL);

        timing->tick = equeue_tick();
        timing->delay = (i + 1) * 100;
        equeue_event_delay(timing, timing->delay);
        equeue_event_period(timing, timing->delay);

        int id = equeue_post(&q, timing_func, timing);
        ASSERT_NE(0, id);
    }

    equeue_dispatch(&q, 2 * ITERATION_TIMES * 100);

    equeue_destroy(&q);
}

/** Test that equeue keeps good time at starting events when events are added via functions already placed in equeue.
 *
 *  Given queue is initialized.
 *  When equeue is being dispatched and new events are added via already placed in equeue.
 *  Then events happen according to the schedule with an error within a specified range.
 */
TEST_F(TestEqueue, test_equeue_fragmenting_barrage)
{
    equeue_t q;
    int err = equeue_create(&q,
                            2 * ITERATION_TIMES * (EQUEUE_EVENT_SIZE + sizeof(struct fragment) + ITERATION_TIMES * sizeof(int)));
    ASSERT_EQ(0, err);

    for (int i = 0; i < ITERATION_TIMES; i++) {
        size_t size = sizeof(struct fragment) + i * sizeof(int);
        struct fragment *fragment = reinterpret_cast<struct fragment *>(equeue_alloc(&q, size));
        ASSERT_TRUE(fragment != NULL);

        fragment->q = &q;
        fragment->size = size;
        fragment->timing.tick = equeue_tick();
        fragment->timing.delay = (i + 1) * 100;
        equeue_event_delay(fragment, fragment->timing.delay);

        int id = equeue_post(&q, fragment_func, fragment);
        ASSERT_NE(0, id);
    }

    equeue_dispatch(&q, ITERATION_TIMES * 100);

    equeue_destroy(&q);
}

/** Test that equeue keeps good time at starting events even if it is working on different thread.
 *
 *  Given queue is initialized.
 *  When equeue is being dispatched on different thread.
 *  Then events happen according to the schedule with an error within a specified range.
 */
TEST_F(TestEqueue, test_equeue_multithreaded_barrage)
{
    equeue_t q;
    int err = equeue_create(&q, ITERATION_TIMES * (EQUEUE_EVENT_SIZE + sizeof(struct timing)));
    ASSERT_EQ(0, err);

    struct ethread t;
    t.q = &q;
    t.ms = ITERATION_TIMES * 100;

    err = pthread_create(&t.thread, 0, ethread_dispatch, &t);
    ASSERT_EQ(0, err);

    for (int i = 0; i < ITERATION_TIMES; i++) {
        struct timing *timing = reinterpret_cast<struct timing *>(equeue_alloc(&q, sizeof(struct timing)));
        ASSERT_TRUE(timing != NULL);

        timing->tick = equeue_tick();
        timing->delay = (i + 1) * 100;
        equeue_event_delay(timing, timing->delay);
        equeue_event_period(timing, timing->delay);

        int id = equeue_post(&q, timing_func, timing);
        ASSERT_NE(0, id);
    }

    err = pthread_join(t.thread, 0);
    ASSERT_EQ(0, err);

    equeue_destroy(&q);
}

/** Test that break request flag is cleared when equeue stops dispatching timeouts.
 *
 *  Given queue is initialized.
 *  When equeue break request flag is called but equeue stops dispatching because of timeout.
 *  Then next equeue dispatch is not stopped.
 */
TEST_F(TestEqueue, test_equeue_break_request_cleared_on_timeout)
{
    equeue_t q;
    int err = equeue_create(&q, TEST_EQUEUE_SIZE);
    ASSERT_EQ(0, err);

    struct count_and_queue pq;
    pq.p = 0;
    pq.q = &q;

    int id = equeue_call_in(&q, 1, simple_breaker, &pq);

    equeue_dispatch(&q, 10);

    EXPECT_EQ(1, pq.p);

    equeue_cancel(&q, id);

    uint8_t touched = 0;
    equeue_call_every(&q, 10, simple_func, &touched);

    equeue_dispatch(&q, 55);
    EXPECT_EQ(5, touched);

    equeue_destroy(&q);
}

/** Test that siblings events don't have next pointers.
 *
 *  Given queue is initialized.
 *  When events are scheduled on the same time.
 *  Then they are connected via sibling pointers and siblings have their next pointer pointing to NULL.
 */
TEST_F(TestEqueue, test_equeue_sibling)
{
    equeue_t q;
    int err = equeue_create(&q, TEST_EQUEUE_SIZE);
    ASSERT_EQ(0, err);

    int id0 = equeue_call_in(&q, 1, pass_func, 0);
    int id1 = equeue_call_in(&q, 1, pass_func, 0);
    int id2 = equeue_call_in(&q, 1, pass_func, 0);

    struct equeue_event *e = q.queue;

    for (; e; e = e->next) {
        for (struct equeue_event *s = e->sibling; s; s = s->sibling) {
            EXPECT_TRUE(s->next == NULL);
        }
    }
    equeue_cancel(&q, id0);
    equeue_cancel(&q, id1);
    equeue_cancel(&q, id2);
    equeue_destroy(&q);
}

/** Test that equeue executes user allocated events passed by equeue_post.
 *
 *  Given queue is initialized and its size is set to store one event at max in its internal memory.
 *  When post events allocated in queues internal memory (what is done by calling equeue_call).
 *  Then only one event can be posted due to queue memory size.
 *  When post user allocated events.
 *  Then number of posted events is not limited by queue memory size.
 *  When both queue allocaded and user allocated events are posted and equeue_dispatch is called.
 *  Then both types of events are executed properly.
 */
TEST_F(TestEqueue, test_equeue_user_allocated_event_post)
{
    struct user_allocated_event {
        struct equeue_event e;
        uint8_t touched;
    };
    equeue_t q;
    int err = equeue_create(&q, EQUEUE_EVENT_SIZE);
    ASSERT_EQ(0, err);

    uint8_t touched = 0;
    user_allocated_event e1 = { { 0, 0, 0, NULL, NULL, NULL, 0, -1, NULL, NULL }, 0 };
    user_allocated_event e2 = { { 0, 0, 0, NULL, NULL, NULL, 10,  10, NULL, NULL }, 0 };
    user_allocated_event e3 = { { 0, 0, 0, NULL, NULL, NULL, 10,  10, NULL, NULL }, 0 };
    user_allocated_event e4 = { { 0, 0, 0, NULL, NULL, NULL, 10,  10, NULL, NULL }, 0 };
    user_allocated_event e5 = { { 0, 0, 0, NULL, NULL, NULL, 0, -1, NULL, NULL }, 0 };

    EXPECT_NE(0, equeue_call_every(&q, 10, simple_func, &touched));
    EXPECT_EQ(0, equeue_call_every(&q, 10, simple_func, &touched));
    EXPECT_EQ(0, equeue_call_every(&q, 10, simple_func, &touched));

    equeue_post_user_allocated(&q, simple_func, &e1.e);
    equeue_post_user_allocated(&q, simple_func, &e2.e);
    equeue_post_user_allocated(&q, simple_func, &e3.e);
    equeue_post_user_allocated(&q, simple_func, &e4.e);
    equeue_post_user_allocated(&q, simple_func, &e5.e);
    equeue_cancel_user_allocated(&q, &e3.e);
    equeue_cancel_user_allocated(&q, &e3.e);

    equeue_dispatch(&q, 11);

    EXPECT_EQ(1, touched);
    EXPECT_EQ(1, e1.touched);
    EXPECT_EQ(1, e2.touched);
    EXPECT_EQ(0, e3.touched);
    EXPECT_EQ(1, e4.touched);
    EXPECT_EQ(1, e5.touched);

    e3.e.target = 10; // set target as it's modified by equeue_call
    e3.e.period = 10; // set period as it's reset by equeue_cancel
    equeue_post_user_allocated(&q, simple_func, &e3.e);
    equeue_dispatch(&q, 101);

    EXPECT_EQ(11, touched);
    EXPECT_EQ(1 , e1.touched);
    EXPECT_EQ(11, e2.touched);
    EXPECT_EQ(10 , e3.touched);
    EXPECT_EQ(11, e4.touched);
    EXPECT_EQ(1 , e5.touched);

    equeue_destroy(&q);
}
