#include "pch.h"
#include <string>
#include <iostream>
#include <thread>
#include<queue>
#include<mutex>
#include<memory>
#include<condition_variable>
#include <string>
#include <chrono>




/////////////////////////////////////Thread-safe queue from lecture 10////////////////////////////////////////////////////

template<typename T>
class FGTSqueue {
private:
	struct node {
		std::shared_ptr <T> data;
		std::unique_ptr<node> next;
	};

	std::mutex head_m;
	std::mutex tail_m;
	std::unique_ptr<node> head;
	node *tail;

	node *get_tail() {
		std::lock_guard<std::mutex> lock(tail_m);
		return tail;

	}

	std::unique_ptr<node> pop_head() {
		std::lock_guard<std::mutex> lock(head_m);
		if (head.get() == get_tail())
			return nullptr;
		std::unique_ptr<node> oh = std::move(head);
		head = std::move(oh->next);
		return oh;
	}

public:
	FGTSqueue() : head(new node), tail(head.get()) {}
	FGTSqueue(const FGTSqueue & other) = delete;

	std::shared_ptr<T> try_pop() {
		std::unique_ptr<node> old_head = pop_head();
		return old_head ? old_head->data : std::shared_ptr<T>();

	}




	void push(T val) {
		std::shared_ptr<T> d(std::make_shared<T>(std::move(val)));
		std::unique_ptr<node> p(new node);
		node*const new_tail = p.get();
		std::lock_guard<std::mutex> lock(tail_m);
		tail->data = d;
		tail->next = std::move(p);
		tail = new_tail;

	}

	bool isEmpty() {
		return head.get() == get_tail();

	}


};



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



typedef int chessboard;
int sol1 = 0;
std::mutex sol1_m;
int breakThreads;

class subBoard
{

public:


	chessboard ld = 0;
	chessboard cols = 0;
	chessboard rd = 0;
	chessboard nextPos = 0;
	chessboard pos;

};
/* Finds the number of solutions to the N-queen problem
 * ld: a bit pattern containing ones for positions under attack along left diagonals for this row
 * cols: a bit pattern containing ones for columns that are already accupied
 * rd: a bit pattern containing ones for positions under attack along right diagonals for this row
 * all: a bit pattern where the first N bits are set to one, where N is the number of queens
 *
 * ld, cols, and rd contain sufficient info about the current state of the board.
 * (ld | cols | rd) is a bit pattern containing ones in all positions under attack for this row
 */


FGTSqueue<subBoard> queue;

void  seq_nqueen(chessboard ld, chessboard cols, chessboard rd, const chessboard all) {			// orginal sequential code with added lock quards and void return instead of int 


	if (cols == all) {                           // A solution is found 
		std::lock_guard<std::mutex> lock(sol1_m);
		sol1 = sol1 + 1;
		return;
	}
	chessboard pos = ~(ld | cols | rd) & all;  // Possible posstions for the queen on the current row     
	chessboard nextPos;
	while (pos != 0) {                          // Iterate over all possible positions and solve the (N-1)-queen in each case
		nextPos = pos & (-pos);                    // next possible position 
		pos -= nextPos;                             // update the possible position 
		seq_nqueen((ld | nextPos) << 1, cols | nextPos, (rd | nextPos) >> 1, all); // recursive call for the `next' position 	   
	}
	return;
}
void  par_nqueen(chessboard ld, chessboard cols, chessboard rd, const chessboard all) {//almost the same as seq_nqueen but with extra check at the end. if more elements in the queue calculate for them


	if (cols == all) {                           // A solution is found 
		std::lock_guard<std::mutex> lock(sol1_m);//quards so no race conditions 
		sol1 = sol1 + 1;
		return;
	}
	chessboard pos = ~(ld | cols | rd) & all;  // Possible posstions for the queen on the current row     
	chessboard nextPos;
	while (pos != 0) {                          // Iterate over all possible positions and solve the (N-1)-queen in each case
		nextPos = pos & (-pos);                    // next possible position 
		pos -= nextPos;                             // update the possible position 
		seq_nqueen((ld | nextPos) << 1, cols | nextPos, (rd | nextPos) >> 1, all); // recursive call for the `next' position 	   
	}
	/*while (!queue.isEmpty()) {//race condition how to get rid?
		std::shared_ptr<subBoard>  a = queue.try_pop();
		par_nqueen(a->ld, a->cols, a->rd, all);
	}*/



	bool test = true;
	while (test) {//if there are still elements in the queue when thread finished, pop next element and calculate solution for that board state     //whats wrong with break?
		std::shared_ptr<subBoard>  a = queue.try_pop();
		if (a == nullptr)
			test = false;
		else {
			seq_nqueen(a->ld, a->cols, a->rd, all);//pretty sure there is no difference between usuing par_nqueen and seq_nqueen here 
		}
	}
	return;
}

int nqueen_solver(int n, int numberOfThreads) {//function that decides between parallel and sequential solution.

	chessboard all = (1 << n) - 1;            // set N bits on, representing number of columns     
	if (n > breakThreads) {						//parallel 

		std::vector<std::thread> vecOfThreads;//creat vector that will hold threads
		chessboard pos = all;
		while (pos != 0) {				 // Iterate over all possible positions and add those board states to queue that then will be used by threads 
			subBoard a;					 //
			a.nextPos = pos & (-pos);	// next possible position 
			a.pos = pos;				//position of new board is 
			a.pos -= a.nextPos;			 // update the possible position 
			pos -= a.nextPos;			//
			a.ld = (a.ld | a.nextPos) << 1;			//	
			a.cols = a.cols | a.nextPos;				//
			a.rd = (a.rd | a.nextPos) >> 1;				//
			queue.push(a);								//
		}


		for (int i = 0; i < numberOfThreads;i++) {				//creat threads until no more elements in queue or until reached maximum numgber of threads 
			std::shared_ptr<subBoard>  a = queue.try_pop();
			if (a == nullptr)
				break;
			vecOfThreads.push_back(std::thread(par_nqueen, a->ld, a->cols, a->rd, all));   // recursive call for the board state stored in queue
		}
		for (int i = 0; i < vecOfThreads.size();i++) {//join all threads 
			vecOfThreads[i].join();
		}
		return sol1;//return number of solutions 
	}
	else { //sequential  solution
		seq_nqueen(0, 0, 0, all);
		return sol1;
	}
}


int main(int argc, char** argv) {


	int qn = std::stoi(argv[1]);//number of queens
	int numberOfThreads = std::stoi(argv[2]);//number of threads	
	breakThreads = std::stoi(argv[3]);// number of queens that if qn>breakThreads use sequential  solution else use parallel  

/* test for 1-18 queens for table for raport
	for (int i = 1;i < 18;i++) {
		qn = i;
		numberOfThreads = 8;
		std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
	int sol = nqueen_solver(qn, numberOfThreads);

	std::cout << "Numer of Solution to " << qn << "-Queen prorblem is : " << sol1 << std::endl;



	std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();


	std::cout <<"queens"<<qn <<"Time difference = " << std::chrono::duration_cast<std::chrono::nanoseconds> (end - begin).count() << "[ns]" << std::endl;
	sol1 = 0;
}*/

	int sol = nqueen_solver(qn, numberOfThreads);
	std::cout << "Numer of Solution to " << qn << "-Queen problem is : " << sol << std::endl;

	return 0;
}
