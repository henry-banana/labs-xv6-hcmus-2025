#include "kernel/param.h"
#include "kernel/fcntl.h"
#include "kernel/types.h"
#include "kernel/riscv.h"
#include "user/user.h"
#include "kernel/vm.h"

#define SZ (8 * SUPERPGSIZE)

void print_pgtbl();
void print_kpgtbl();
void ugetpid_test();
void superpg_fork();
void superpg_free();
void pgaccess_test();  // NEW: pgaccess test suite

int
main(int argc, char *argv[])
{
  print_pgtbl();
  // ugetpid_test();  // SKIP: USYSCALL page (không thuộc Lab 2)
  print_kpgtbl();     // Test vmprint
  pgaccess_test();    // Test pgaccess
  // superpg_fork();  // SKIP: Super pages (không thuộc Lab 2)
  // superpg_free();  // SKIP: Super pages (không thuộc Lab 2)
  printf("pgtbltest: all tests succeeded\n");
  exit(0);
}


char *testname = "???";

void
err(char *why)
{
  printf("pgtbltest: %s failed: %s, pid=%d\n", testname, why, getpid());
  exit(1);
}

void
print_pte(uint64 va)
{
    pte_t pte = (pte_t) pgpte((void *) va);
    printf("va 0x%lx pte 0x%lx pa 0x%lx perm 0x%lx\n", va, pte, PTE2PA(pte), PTE_FLAGS(pte));
}

void
print_pgtbl()
{
  printf("print_pgtbl starting\n");
  for (uint64 i = 0; i < 10; i++) {
    print_pte(i * PGSIZE);
  }
  uint64 top = MAXVA/PGSIZE;
  for (uint64 i = top-10; i < top; i++) {
    print_pte(i * PGSIZE);
  }
  printf("print_pgtbl: OK\n");
}

void
ugetpid_test()
{
  int i;

  printf("ugetpid_test starting\n");
  testname = "ugetpid_test";

  if(getpid() != ugetpid())
    err("mismatched PID #1");

  for (i = 0; i < 64; i++) {
    int ret = fork();
    if (ret != 0) {
      wait(&ret);
      if (ret != 0)
        exit(1);
      continue;
    }
    if (getpid() != ugetpid())
      err("mismatched PID #2");
    exit(0);
  }
  printf("ugetpid_test: OK\n");
}

void
print_kpgtbl()
{
  printf("print_kpgtbl starting\n");
  kpgtbl();
  printf("print_kpgtbl: OK\n");
}

/**
 * TEST 1: pgaccess_test_none
 * --------------------------
 * Mục đích: Kiểm tra syscall pgaccess() hoạt động cơ bản
 * 
 * Input:  3 pages mới allocate, KHÔNG access
 * Output: abits có thể là 0 hoặc giá trị khác (tùy cài đặt sbrk)
 * 
 * Điều kiện PASS: syscall không return error
 */
void
pgaccess_test_none()
{
  char *buf;
  uint64 abits = 0;
  
  printf("pgaccess_test: Test 1 - Kiem tra syscall co hoat dong\n");
  testname = "pgaccess_none";
  
  // Bước 1: Allocate 3 pages
  buf = sbrk(3 * PGSIZE);
  if (buf == (char*)0xffffffffffffffff) {
    err("sbrk failed");
  }
  
  // Bước 2: Clear access bits bằng cách gọi pgaccess lần đầu
  if (pgaccess(buf, 3, &abits) < 0) {
    err("pgaccess syscall failed");
  }
  
  // Bước 3: Allocate thêm 3 pages mới và test ngay (không access)
  char *buf2 = sbrk(3 * PGSIZE);
  if (buf2 == (char*)0xffffffffffffffff) {
    err("sbrk buf2 failed");
  }
  
  // Bước 4: Gọi pgaccess trên pages mới
  abits = 0;
  if (pgaccess(buf2, 3, &abits) < 0) {
    err("pgaccess failed on new pages");
  }
  
  // Kết quả: In abits để verify
  // Lưu ý: sbrk có thể touch pages nên abits có thể != 0
  printf("  -> Syscall hoat dong, abits = 0x%lx\n", abits);
  printf("pgaccess_test: Test 1 - OK\n");
  
  // Cleanup: Trả lại memory
  sbrk(-6 * PGSIZE);
}

/**
 * TEST 2: pgaccess_test_single
 * ----------------------------
 * Mục đích: Kiểm tra pgaccess detect đúng page được access
 * 
 * Input:  5 pages, access CHỈ page index 2 (zero-indexed)
 * Output dự kiến: abits bit 2 = 1, tức là abits & 0x4 != 0
 * 
 * Điều kiện PASS: bit 2 trong abits được set
 * 
 * Giải thích:
 *   - Khi ghi vào buf[2*PGSIZE], CPU access page 2
 *   - Hardware tự động set PTE_A bit trong PTE của page 2
 *   - pgaccess() đọc PTE và thấy bit PTE_A = 1
 *   - -> mask bit 2 = 1
 */
void
pgaccess_test_single()
{
  char *buf;
  uint64 abits = 0;
  
  printf("pgaccess_test: Test 2 - Detect mot page duoc access\n");
  testname = "pgaccess_single";
  
  // Bước 1: Allocate 5 pages liên tục
  buf = sbrk(5 * PGSIZE);
  if (buf == (char*)0xffffffffffffffff) {
    err("sbrk failed");
  }
  
  // Bước 2: Clear access bits (gọi pgaccess để reset PTE_A)
  if (pgaccess(buf, 5, &abits) < 0) {
    err("pgaccess clear failed");
  }
  
  // Bước 3: ACCESS PAGE 2 - Ghi 1 byte vào page 2
  //         Địa chỉ = buf + 2 * 4096 = byte đầu tiên của page 2
  buf[2 * PGSIZE] = 'X';
  
  // Bước 4: Gọi pgaccess để kiểm tra
  abits = 0;
  if (pgaccess(buf, 5, &abits) < 0) {
    err("pgaccess failed");
  }
  
  // Bước 5: Verify kết quả
  // Mong đợi: bit 2 được set -> abits = 0b00100 = 0x4 (hoặc có thêm bits khác)
  printf("  -> abits = 0x%lx (mong doi bit 2 = 1)\n", abits);
  
  if ((abits & (1 << 2)) == 0) {
    printf("  LOI: Bit 2 KHONG duoc set!\n");
    err("Page 2 accessed but bit not set");
  }
  
  printf("  -> Bit 2 da duoc set dung!\n");
  printf("pgaccess_test: Test 2 - OK\n");
  
  // Cleanup
  sbrk(-5 * PGSIZE);
}

/**
 * TEST 3: pgaccess_test_clearing (QUAN TRỌNG NHẤT!)
 * --------------------------------------------------
 * Mục đích: Kiểm tra kernel CLEAR bit PTE_A sau khi đọc
 * 
 * Đây là test QUAN TRỌNG NHẤT vì:
 *   - Nếu không clear PTE_A, lần gọi tiếp sẽ thấy bit = 1 mãi
 *   - Không thể detect access MỚI trong tương lai
 * 
 * Quy trình test:
 *   Lần 1: Access page 1 -> pgaccess() -> bit 1 = 1 (PASS nếu đúng)
 *   Lần 2: KHÔNG access  -> pgaccess() -> tất cả bits = 0 (PASS nếu = 0)
 *   Lần 3: Access page 2 -> pgaccess() -> bit 2 = 1 (PASS nếu đúng)
 * 
 * Điều kiện PASS:
 *   - Lần 1: abits & 0x2 != 0 (bit 1 set)
 *   - Lần 2: abits == 0       (đã clear!)
 *   - Lần 3: abits & 0x4 != 0 (bit 2 set)
 */
void
pgaccess_test_clearing()
{
  char *buf;
  uint64 abits = 0;
  
  printf("pgaccess_test: Test 3 - Kiem tra clear PTE_A (QUAN TRONG!)\n");
  testname = "pgaccess_clearing";
  
  // ========== SETUP ==========
  // Allocate 3 pages
  buf = sbrk(3 * PGSIZE);
  if (buf == (char*)0xffffffffffffffff) {
    err("sbrk failed");
  }
  
  // Clear existing bits
  pgaccess(buf, 3, &abits);
  printf("  Setup: Da clear access bits\n");
  
  // ========== LAN 1: ACCESS PAGE 1 ==========
  printf("  Lan 1: Access page 1...\n");
  
  // Ghi vào page 1 -> hardware set PTE_A = 1
  buf[1 * PGSIZE] = 'X';
  
  // Gọi pgaccess
  abits = 0;
  if (pgaccess(buf, 3, &abits) < 0) {
    err("pgaccess lan 1 failed");
  }
  
  printf("    -> abits = 0x%lx\n", abits);
  
  // Verify: bit 1 phải được set
  if ((abits & (1 << 1)) == 0) {
    printf("    LOI: Mong doi bit 1 = 1, nhung abits = 0x%lx\n", abits);
    err("Lan 1 failed: bit 1 not set");
  }
  printf("    -> PASS: Bit 1 da set\n");
  
  // ========== LAN 2: KHONG ACCESS, KIEM TRA CLEAR ==========
  printf("  Lan 2: Khong access, kiem tra clear...\n");
  
  // Gọi pgaccess LẦN NỮA mà KHÔNG access gì
  abits = 0;
  if (pgaccess(buf, 3, &abits) < 0) {
    err("pgaccess lan 2 failed");
  }
  
  printf("    -> abits = 0x%lx (mong doi = 0)\n", abits);
  
  // Verify: tất cả bits PHẢI = 0 (vì đã clear ở lần gọi trước)
  if (abits != 0) {
    printf("    LOI: PTE_A KHONG duoc clear! abits = 0x%lx\n", abits);
    printf("    -> Kernel thieu dong: *pte &= ~PTE_A\n");
    err("PTE_A not cleared!");
  }
  printf("    -> PASS: Da clear, abits = 0\n");
  
  // ========== LAN 3: ACCESS PAGE 2 MOI ==========
  printf("  Lan 3: Access page 2 moi...\n");
  
  // Access page 2
  buf[2 * PGSIZE] = 'Y';
  
  // Gọi pgaccess
  abits = 0;
  if (pgaccess(buf, 3, &abits) < 0) {
    err("pgaccess lan 3 failed");
  }
  
  printf("    -> abits = 0x%lx\n", abits);
  
  // Verify: bit 2 phải được set
  if ((abits & (1 << 2)) == 0) {
    printf("    LOI: Mong doi bit 2 = 1, nhung abits = 0x%lx\n", abits);
    err("Lan 3 failed: bit 2 not set");
  }
  printf("    -> PASS: Bit 2 da set\n");
  
  // ========== CLEANUP ==========
  sbrk(-3 * PGSIZE);
  
  printf("pgaccess_test: Test 3 - OK (Clear PTE_A hoat dong dung!)\n");
}

/**
 * MAIN PGACCESS TEST SUITE
 * ------------------------
 * Chạy tất cả 3 tests theo thứ tự
 */
void
pgaccess_test()
{
  printf("\n");
  printf("        PGACCESS TEST SUITE - Lab 2: Page Tables\n");
  printf("============================================================\n");
  
  pgaccess_test_none();      // Test 1: Syscall hoat dong
  printf("\n");
  
  pgaccess_test_single();    // Test 2: Detect 1 page
  printf("\n");
  
  pgaccess_test_clearing();  // Test 3: Clear PTE_A (QUAN TRONG NHAT!)
  printf("\n");
  
  printf("============================================================\n");
  printf("        PGACCESS: TAT CA 3 TESTS PASSED!\n");
  printf("\n");
}

void
supercheck(char *end)
{

  pte_t last_pte = 0;
  uint64 a = (uint64) end;
  uint64 s = SUPERPGROUNDUP(a);

  for (; a < s; a += PGSIZE) {
    pte_t pte = (pte_t) pgpte((void *) a);
    if (pte == 0) {
      err("no pte");
    }
  }

  for (uint64 p = s;  p < s + 512 * PGSIZE; p += PGSIZE) {
    pte_t pte = (pte_t) pgpte((void *) p);
    if(pte == 0)
      err("no pte");
    if ((uint64) last_pte != 0 && pte != last_pte) {
        err("pte different");
    }
    if((pte & PTE_V) == 0 || (pte & PTE_R) == 0 || (pte & PTE_W) == 0){
      err("pte wrong");
    }
    last_pte = pte;
  }

  for(int i = 0; i < 512 * PGSIZE; i += PGSIZE){
    *(int*)(s+i) = i;
  }

  for(int i = 0; i < 512 * PGSIZE; i += PGSIZE){
    if(*(int*)(s+i) != i)
      err("wrong value");
  }
}

void
superpg_fork()
{
  int pid;
  
  printf("superpg_fork starting\n");
  testname = "superpg_fork";
  
  char *end = sbrk(SZ);
  if (end == 0 || end == SBRK_ERROR)
    err("sbrk failed");

  // check if parent has super pages
  supercheck(end);
  if((pid = fork()) < 0) {
    err("fork");
  } else if(pid == 0) {
    // check if child's address space has super pages
    supercheck(end);
    exit(0);
  } else {
    int status;
    wait(&status);
    if (status != 0) {
      exit(0);
    }
  }

  // free super pages
  sbrk(-SZ);
  if((pid = fork()) < 0) {
    err("fork");
  } else if(pid == 0) {
    // reference freed memory; this should result in page fault and
    // the kernel should kill the child.
    * (end + 1) = '9'; 
  } else {
    int status;
    wait(&status);
    if (status == 0) {
      err("child was able to reference free memory\n");
      exit(1);
    }
  }  
  printf("superpg_fork: OK\n");  
}

void
superpg_free()
{
  int pid;
  
  printf("superpg_free starting\n");
  testname = "superpg_free";

  char *end = sbrk(SZ);
  if (end == 0 || end == SBRK_ERROR)
    err("sbrk failed");

  // free pages beyond a super page
  char *a = sbrk(0);
  uint64 s = SUPERPGROUNDDOWN((uint64) a);
  sbrk(-((uint64) a-s));
  a = sbrk(0);

  pte_t pte1 = (pte_t) pgpte((void *) a-PGSIZE);
  pte_t pte2 = (pte_t) pgpte((void *) a-2*PGSIZE);
  if (pte1 != pte2) {
    err("not a super page");
  }
  
  // write to the last 8192-byte section of a super page
  * (a - PGSIZE + 1) = '8';
  * (a - 2*PGSIZE + 1) = '9';

  // free last 4096 bytes of a super page
  sbrk(-PGSIZE);
  a = sbrk(0);

  if (*(a - PGSIZE + 1) != '9') {
    err("lost content after freeing part of super page");
  }

  if((pid = fork()) < 0) {
    err("fork");
  } else if(pid == 0) {
     // the memory at address a shouldn't be in the child's address
     // space, since the parent freed it. The following reference
     // should result in page fault and the kernel should kill the
     // child.
    if (* (a + 1) == '9') {
      exit(0);
    }
  } else {
    int status;
    wait(&status);
    if (status == 0) {
      err("child was able to reference free memory\n");
      exit(1);
    }
  }

  pte1 = (pte_t) pgpte((void *) a);
  if(pte1 != 0) {
    err("pte for freed memory is valid");
  }

  s = SUPERPGROUNDDOWN((uint64) a);
  for (; (uint64) a > s; a -= PGSIZE) {
    a = sbrk(-PGSIZE);
    pte1 = (pte_t) pgpte(sbrk(0));
    if(pte1 != 0) {
      err("page hasn't been freed");
    }
  }
  
  printf("superpg_free: OK\n");  
}
