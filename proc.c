#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

struct procLog
{
  int pid;
  char name[16];
  int ctime;
  int rtime;
  int etime;
};

struct procLog log[NPROC];

#ifdef RR
int sched_option = 1; // RR
#endif

#ifdef GRT
int sched_option = 100;
#endif

#ifdef QQQ
int sched_option = 1000;
#endif

#ifdef NBF
int boot_first = 0;
#else
int boot_first = 1;
#endif



#ifdef FRR

// create a queue of pids

int sched_option = 10;

int queue[NPROC];
int head_index = -1;
int tail_index = -1;


int queue_size()
{
  if(tail_index >= head_index)
    return tail_index - head_index;
  else
    return NPROC - (head_index - tail_index);
}

int q_contains(int proc_id)
{
  int q_size = queue_size();
  for(int i = 0; i < q_size; i++)
  {
    if(proc_id == queue[(head_index + i) % NPROC]){
      return (head_index + i) % NPROC;
    }
  }
  return -1;
}

void print_queue()
{
  int size = queue_size();


  cprintf("\n");
  for(int i = 0; i < size; i++)
    cprintf("<%d> ", queue[(head_index + i) % NPROC]);
  cprintf("\n");
}

void push_a_proc(int proc_id, int print) // look for instances where a proc was made runnable and push them to queue insted
{
  if(0 < q_contains(proc_id)) // if process exists
    return;
  
  tail_index = (tail_index + 1) % NPROC;
  queue[tail_index] = proc_id;
  if(print > 0)
    print_queue();
}

int pop_a_proc(void)  // change a proc state to running
{
  // cprintf("poping\t\t");
  print_queue();
  head_index = (head_index + 1) % NPROC;
  return queue[head_index];
}


#endif


#ifdef QQQ


int queue[NPROC];
int head_index = -1;
int tail_index = -1;

int queue_size()
{
  if(tail_index >= head_index)
    return tail_index - head_index;
  else
    return NPROC - (head_index - tail_index);
}

int q_contains(int proc_id)
{
  int q_size = queue_size();
  for(int i = 0; i < q_size; i++)
  {
    if(proc_id == queue[(head_index + i) % NPROC]){
      return (head_index + i) % NPROC;
    }
  }
  return -1;
}

void print_queue()
{
  int size = queue_size();

  cprintf("\n");
  for(int i = 0; i < size; i++)
    cprintf("<%d> ", queue[(head_index + i) % NPROC]);
  cprintf("\n");
}

void push_a_proc(int proc_id, int print) 
{
  if(0 < q_contains(proc_id)) // if process exists
    return;
  
  tail_index = (tail_index + 1) % NPROC;
  queue[tail_index] = proc_id;
  if(print > 0)
    print_queue();
}

int pop_a_proc(void)  
{
  print_queue();
  head_index = (head_index + 1) % NPROC;
  return queue[head_index];
}

int remove(int pid)
{
  int found_index = q_contains(pid); 
  if( found_index >= 0)
  {

  }
  else
  {
    cprintf("pid is not in queue: %d", pid);
    return -1;
  }
  
  return 1;
}

#endif


void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  // track ticks
  p->ctime = ticks;
  p->rtime = 0;
  p->sched_tick_c = 0;
  p->priority = HIGH;
  
  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  p->priority = HIGH;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

    #ifdef FRR
	// defenition new scheduling
    push_a_proc(p->pid, 1);
    
	#endif
  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;
  np->priority = curproc->priority;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;
  
  #ifdef FRR
    push_a_proc(np->pid, 1);
  #endif

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  struct procLog l;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;
  // we consider here is the end of process
  curproc->etime = ticks;
  
  /* saving dead process */
  l.pid = curproc->pid;
  int c_count;
  for(c_count = 0; c_count < 16; c_count++) // simplest way to copy a string as a char array
    l.name[c_count] = curproc->name[c_count];
  l.rtime = curproc->rtime;
  l.etime = curproc->etime;
  l.ctime = curproc->ctime;
  log[curproc->pid] = l;
  /* /saving dead process */
  
  
  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
		p->priority = LOW;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
		p->etime = 0;
        p->rtime = 0;
        p->ctime = 0;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

/* start of prformance recording */
int performance_recording(int *wtime, int *rtime)
{
  // copy wait() here and then add some code to fill out pointers

  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        // my additions to wait to get performance data
        *rtime = p->rtime; // value of rtime is now rtime of p
        *wtime = p->etime - p->rtime - p->ctime; // wait time is end time - create time - run time  
        //
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        // this function clears the page table
        // this should have been the reason for ps showing the last end time :)
        p->etime = 0;
        p->rtime = 0;
        p->ctime = 0;
        // reset to low
        p->priority = LOW;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }

}
/* /end of prformance recording */

void RR_policy(struct proc *p, struct cpu *c)
{
      
  // Loop over prWocess table looking for process to run.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if(p->state != RUNNABLE)
      continue;

    c->proc = p;
    switchuvm(p);
    p->state = RUNNING;
    
    swtch(&(c->scheduler), p->context);
    switchkvm();

    // time priod finished.
    c->proc = 0;
  }
}

void FRR_policy(struct proc *p, struct cpu *c)
{
  #ifdef FRR
  //same as RR
  int this_turn_proc_id;


  if(tail_index != head_index) // if queue is not empty  
  {
    this_turn_proc_id = pop_a_proc();
  }
  else
  { 
    return;
  }
  // loop over proc table to find chosen process
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if(p->state != RUNNABLE)
      continue;

    if(p->pid != this_turn_proc_id)
      continue;

	/* switching should happen in the way that
	process release ptable.lock */
    c->proc = p;
    switchuvm(p);
    p->state = RUNNING;

    
    swtch(&(c->scheduler), p->context);
    switchkvm();

	//same as RR 
    c->proc = 0;
  }
  #endif
  return;
}
void GRT_policy(struct proc *p, struct cpu *c)
{
  #ifdef GRT

  struct proc *process_selection = 0;
  int min_share = 100000;

  // Loop over process table looking for process to run.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if(p->state == RUNNABLE)
      if(process_selection != 0)
      {
        // find share
        int number_to_divide = ticks - p->ctime;
        int share = 100000000; 
        if(0 != number_to_divide) 
          share = p->rtime / number_to_divide;
        if(share < min_share)
        {
          min_share = share;
          process_selection = p;
        }
      }
      else
        process_selection = p;
    else
    {
      continue;
    }
  }
  // process found
  if(process_selection != 0)
  {
    p = process_selection;

    c->proc = p;
    switchuvm(p);
    p->state = RUNNING;
    
    swtch(&(c->scheduler), p->context);
    switchkvm();

    // same as RR
    c->proc = 0;
  }
  #endif

  // if nothing was defined just return :) or just by reaching the end
  return;
}

int find_high_priority(void)
{
    struct proc *p;

    // acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
        if(p->state != RUNNABLE)
            continue;
        if(p->priority == HIGH)
        {
            cprintf("process with highest priority %d, %s\n", p->pid, stringFromPriority(p->priority));
            return 1;
        }
    }
    return -1;
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p = 0;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // interupts defined
    sti();

    #ifdef FRR 
          if(tail_index == head_index)
          {
            acquire(&ptable.lock);
            for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
            {
              if(p->state == RUNNABLE)
              {
                push_a_proc(p->pid, 1);
              }
            }
            release(&ptable.lock);
          }
    #endif

    #ifdef QQQ
          if(tail_index == head_index)
          {
            acquire(&ptable.lock);
            for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
            {
              if(p->state == RUNNABLE && p->priority == MID)
              {
                push_a_proc(p->pid, 1);
              }
            }
            release(&ptable.lock);
          }
    #endif
    // as seen in all policies
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);

      if(nextpid < 4 && boot_first == 1)
      {

          // Loop over process table looking for process to run.
          for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
          {
              if(p->state != RUNNABLE)
                  continue;

              
              c->proc = p;
              switchuvm(p);
              p->state = RUNNING;

              cprintf("bootup process %s with pid %d is now running\n", p->name, p->pid);
              // ... ... ...

              swtch(&(c->scheduler), p->context);
              switchkvm();

              // Process is done running for now.
              // It should have changed its p->state before coming back.
              c->proc = 0;
          }
      }
      else
      {
          switch (sched_option)
          {
              case 1: // RR
                  RR_policy(p, c);
                  break;

              case 10:
                  FRR_policy(p, c);
                  break;

              case 100:
                  GRT_policy(p, c);
                  break;

              case 1000:
                  // i fear that some dont change their priorty and we get stuck
        #ifdef QQQ
                          // check if there is any process with high priority
              if(check_high_p_exists() >= 0)
              {
                //todo: make it cleaner
                //for now its just a copy of GRT
                struct proc *minP = 0;
                int min_share = 10000000;

                for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
                {
                  // check if priority is high
                  if(p->state == RUNNABLE && p->priority == HIGH)
                    if(minP != 0)
                    {
                      int divide_to = ticks - p->ctime;
                      int share = 1000000000;
                      if(0 != divide_to)
                        share = p->rtime / divide_to;
                      if(share < min_share)
                      {
                        min_share = share;
                        minP = p;
                      }
                    }
                    else
                      minP = p;
                  else
                  {
                    continue;
                  }
                }
                if(minP != 0)
                {
                  p = minP;

                  c->proc = p;
                  switchuvm(p);
                  p->state = RUNNING;

                  swtch(&(c->scheduler), p->context);
                  switchkvm();

                  c->proc = 0;
                }
              }
              else if (head_index != tail_index)
              {
                int this_turn_proc_id = pop_a_proc();

                // loop over proc table to find chosen process

                for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
                {
                  if(p->state != RUNNABLE)
                    continue;

                  if(p->pid != this_turn_proc_id)
                    continue;

                  c->proc = p;
                  switchuvm(p);
                  p->state = RUNNING;

                  swtch(&(c->scheduler), p->context);
                  switchkvm();

                  c->proc = 0;
                }
              }
              else
              {
                for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
                {
                  if(p->state != RUNNABLE)
                    continue;

                  c->proc = p;
                  switchuvm(p);
                  p->state = RUNNING;

                  swtch(&(c->scheduler), p->context);
                  switchkvm();

                  c->proc = 0;
                }
              }
        #endif
                  break;
              // previous loop never happens
//        for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
//          if(p->state != RUNNABLE)
//            continue;
            default:
                break;
        }
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

	  cprintf(" %s with pid %d is running\n", p->name, p->pid);
	  
      swtch(&(c->scheduler), p->context);
      switchkvm();

    //process end (like RR :)) )
      c->proc = 0;
    }
    release(&ptable.lock);

  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  myproc()->sched_tick_c = 0; //added line
  #ifdef FRR
    push_a_proc(myproc()->pid, 1);
  #endif
  #ifdef QQQ
    if(myproc()->priority == MID)
      push_a_proc(myproc()->pid, 1);
  #endif
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan){
      p->state = RUNNABLE;
	  p->state = RUNNABLE;
	  //wakeup for processes added here
      #ifdef FRR
      push_a_proc(p->pid, -1); 
      #endif
      #ifdef QQQ
      if(p->priority == MID)
        push_a_proc(p->pid, -1);
      #endif
	}
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING){
        p->state = RUNNABLE;
		// lines for sched
		#ifdef FRR
        push_a_proc(p->pid, 1);
        #endif
	  }
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

void updateStats() {
  struct proc *p;
  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    switch(p->state) {
      case RUNNING:
        p->rtime++;
        break;
      default:;
    }
  }
  release(&ptable.lock);
} 

int tickcounter()
{
  int returnMe;
  acquire(&ptable.lock); // crititcal section to use the process table
  returnMe = ++myproc()->sched_tick_c;
  release(&ptable.lock);
  return returnMe;
} 
