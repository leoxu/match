#include <map>
#include <set>
#include <list>
#include <cmath>
#include <ctime>
#include <deque>
#include <queue>
#include <stack>
#include <string>
#include <bitset>
#include <cstdio>
#include <limits>
#include <vector>
#include <climits>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <numeric>
#include <sstream>
#include <iostream>
#include <iterator>
#include <algorithm>
#include <functional>
#include <unordered_map>
#include <assert.h>

enum BUY_SELL { TYPE = 1, PRICE = 2 , QUANTITY = 3, ID = 4 };
enum MODIFY   { MODIFY_ID = 1, MODIFY_OP = 2, MODIFY_PRICE = 3, MODIFY_QUANTITY = 4 };
enum CANCEL   { CANCEL_ID = 1 };

#define TOKEN_MAX_SIZE   5

using namespace std;
struct order {
	unsigned time;
	int      price;
	int      quantity;
	string   id;
};

static bool buy_order_cmp (const order * a, const order * b)
{
	if (a->price > b->price) return true;
	if (a->price < b->price) return false;
	return a->time < b->time;
}
static bool sell_order_cmp (const order * a, const order * b)
{
	if (a->price < b->price) return true;
	if (a->price > b->price) return false;
	return a->time < b->time;
}
// static bool order_find (const order * a, const order * b)
// {
// 	if (a->price > b->price) return true;
// 	return false;
// }
static bool time_find (const order * a, const order * b)
{
	if (a->time < b->time) return true;
	return false;
}

struct order_queue {
	std::vector<order *> time_sorted;
	std::vector<order *> price_sorted;
	std::map<std::string, order *> mapid;

	~order_queue() {
		for(auto o : time_sorted) {
			delete o;
		}
	}
	order * get_first_order() {
		if (!price_sorted.size())
			return NULL;
		return price_sorted[0];
	}

	order * find_order(const std::string & id) {
		auto f = mapid.find(id);
		if (f == mapid.end())
			return nullptr;
		return f->second;
	}
	template<class Cmp>
	void _add_order(struct order * order, Cmp cmp) {
		time_sorted.push_back(order);
		auto i = std::lower_bound(price_sorted.begin(), price_sorted.end(),
				order, cmp);
		price_sorted.insert(i, order);
		mapid[order->id] = order;
	}

	template<class Cmp>
	void _remove_order(const std::string & id, Cmp cmp) {
		auto f = mapid.find(id);
		if (f == mapid.end())
			return ;
		order * order = f->second;
		mapid.erase(f);

		{   // remove order in price_sorted list
			auto low = std::lower_bound(price_sorted.begin(), price_sorted.end(),
				order, cmp);
			auto up = std::upper_bound(price_sorted.begin(), price_sorted.end(),
					order, cmp);
			while(low != up && (*low) != order) {
				low ++;
			}
			if (low != up) {
				assert(*low == order);
				price_sorted.erase(low);
			}
		}
		{   // remove order in time_sorted list
			auto low = std::lower_bound(time_sorted.begin(), time_sorted.end(),
					order, time_find);
			assert( low!= time_sorted.end());
			assert( *low == order);
			time_sorted.erase(low);
		}
	}

};

struct sell_queue : public order_queue {
	void add_order(struct order * order) {
		_add_order(order, sell_order_cmp);
	}
	void remove_order(const std::string & id) {
		_remove_order(id, sell_order_cmp);
	}
};

struct buy_queue : public order_queue {
	void add_order(struct order * order) {
		_add_order(order, buy_order_cmp);
	}
	void remove_order(const std::string & id) {
		_remove_order(id, buy_order_cmp);
	}
};

static void trade_order(struct order * sell_order,struct order * buy_order)
{
	assert(sell_order->price <= buy_order->price);
	// check output order according to time
	struct order * o[2];
	if (sell_order->time < buy_order->time) {
		o[0] = sell_order;
		o[1] = buy_order;
	} else {
		o[1] = sell_order;
		o[0] = buy_order;
	}
	int quantity = std::min(sell_order->quantity, buy_order->quantity);

	std::cout << "TRADE "
			  << o[0]->id << " " << o[0]->price << " " << quantity << " "
			  << o[1]->id << " " << o[1]->price << " " << quantity << " "
			  << std::endl;
	// update quantity
	sell_order->quantity -= quantity;
	buy_order->quantity  -= quantity;
}


static void buy_order(struct order * buy_order, bool gfd,
			   struct sell_queue * sell_queue,
			   struct buy_queue * buy_queue)
{
	order * sell_order;
	// keep consuming sell orders if a matched sell order is available.
	while(buy_order->quantity &&
		  (sell_order = sell_queue->get_first_order()) &&
		  sell_order->price <= buy_order->price) {
		// trade order
		trade_order(sell_order, buy_order);
		// remove sell order if quantity is 0
		if (sell_order->quantity == 0) {
			sell_queue->remove_order(sell_order->id);
		} else {
			assert(buy_order->quantity == 0);
		}
	}
	// save buy order for GFD type
	if (buy_order->quantity && gfd) {
		// put it to buy queue
		buy_queue->add_order(buy_order);
	}
}

static void sell_order(struct order * sell_order, bool gfd,
			   struct sell_queue * sell_queue,
			   struct buy_queue * buy_queue)
{
	order * buy_order;
	// keep consuming buy orders if a matched buy order is available.
	while(sell_order->quantity &&
		  (buy_order = buy_queue->get_first_order()) &&
		  sell_order->price <= buy_order->price) {
		// trade order
		trade_order(sell_order, buy_order);

		if (buy_order->quantity == 0) {
			buy_queue->remove_order(buy_order->id);
		} else {
			assert(sell_order->quantity == 0);
		}
	}
	if (sell_order->quantity && gfd) {
		// put it to sell queue
		sell_queue->add_order(sell_order);
	}
}

static void print_orders(const std::string & msg,
				 std::vector<order *> & orders)
{
	std::cout << msg << std::endl;
	std::unordered_map<int, std::pair<int,bool>> hash;
	for(auto o : orders) {
		auto f = hash.find(o->price);
		if (f == hash.end())
			hash.insert(std::make_pair(o->price, std::make_pair(o->quantity, false)));
		else
			f->second.first += o->quantity;
	}
	for(auto o : orders) {
		if (!hash[o->price].second) {
			std::cout << o->price << " " << hash[o->price].first << std::endl;
			hash[o->price].second = true;
		}
	}
}
#if DEBUG_DUMP
static void dump_order(const order * o)
{
	std::cout << "time=" << o->time
			  << " price=" << o->price
			  << " quantity=" << o->quantity
			  <<" id=" << o->id
			  << std::endl;
}
static void dump_queue(const std::string & msg, struct order_queue * q)
{
	std::cout << msg << " {" << std::endl;

	std::cout << "time sorted:" << std::endl;
	for(int i=0; i< q->time_sorted.size(); i++) {
		dump_order(q->time_sorted[i]);
	}
	std::cout << "price sorted:" << std::endl;
	for(int i=0; i< q->price_sorted.size(); i++) {
		dump_order(q->price_sorted[i]);
	}
	std::cout << msg << " }" << std::endl;
}

static void test(buy_queue & bq, sell_queue & sq)
{
	for(int i=0; i< 16; i++) {
		order * o = new order;
		o->time  = i;
		o->price = 100 + i / 4;
		o->quantity = 10;
		o->id = std::to_string(i);

		bq.add_order(o);
		sq.add_order(o);
	}
	std::cout << "buy queue" << std::endl;
	dump_queue(&bq);

	std::cout << "sell queue" << std::endl;
	dump_queue(&sq);

	for(int k=0; k<16; k++) {
		bq.remove_order(std::to_string(k));
		std::cout << "buy queue remove :" << k << std::endl;
		dump_queue(&bq);
	}

	for(int k=0; k<16; k++) {
		sq.remove_order(std::to_string(k));
		std::cout << "sell queue remove :" << k << std::endl;
		dump_queue(&sq);
	}
}
#endif

int main(int argc, char **argv)
{
	/* Enter your code here. Read input from STDIN. Print output to STDOUT */

	buy_queue  bq;
	sell_queue sq;

	unsigned time = 0;
	std::string input;

	while (std::getline(std::cin, input)) {

		std::istringstream iss(input), end;
		std::vector<std::string> tokens(TOKEN_MAX_SIZE);

		std::copy(std::istream_iterator<std::string>(iss),
				std::istream_iterator<std::string>(end),
				tokens.begin());

		if (tokens[0] == "BUY" || tokens[0] == "SELL") {
			try {
				std::string type = tokens[TYPE];
				order * o = new order;
				if (type == "GFD" || type == "IOC") {
					o->time     = time ++;
					o->price    = std::stoi(tokens[PRICE]);
					o->quantity = std::stoi(tokens[QUANTITY]);
					o->id       = tokens[ID];

					if (tokens[0] == "BUY")
						buy_order(o,  type == "GFD", &sq, &bq);
					else
						sell_order(o, type == "GFD", &sq, &bq);
				}
			} catch(...) {
			}
#if DEBUG_DUMP
			dump_queue(tokens[0] + " sell", &sq);
			dump_queue(tokens[0] + " buy ", &bq);
#endif
		} else if (tokens[0] == "CANCEL") {
			order * o = sq.find_order(tokens[CANCEL_ID]);
			if (o)
				sq.remove_order(o->id);
			else if ((o = bq.find_order(tokens[CANCEL_ID])))
				bq.remove_order(o->id);
#if DEBUG_DUMP
			dump_queue(tokens[0] + " sell", &sq);
			dump_queue(tokens[0] + " buy ", &bq);
#endif
		} else if (tokens[0] == "MODIFY") {
			try {
				int price  = std::stoi(tokens[MODIFY_PRICE]);
				int quantity = std::stoi(tokens[MODIFY_QUANTITY]);
				if (tokens[MODIFY_OP] == "BUY" ||
					tokens[MODIFY_OP] == "SELL" ){
					order * o = sq.find_order(tokens[MODIFY_ID]);
					if (o)
						sq.remove_order(tokens[ID]);
					else if ((o = bq.find_order(tokens[MODIFY_ID]))) {
						bq.remove_order(tokens[MODIFY_ID]);
					}
					if (o && tokens[MODIFY_OP] == "SELL") {
						o->price    = price;
						o->quantity = quantity;
						o->time     = time ++;
						sell_order(o, true, &sq, &bq);
					} else if (o && tokens[MODIFY_OP] == "BUY") {
						o->price    = price;
						o->quantity = quantity;
						o->time     = time ++;
						buy_order(o, true, &sq, &bq);
					}
				}
			} catch (...) {
			}
#if DEBUG_DUMP
			dump_queue("after modify sell", &sq);
			dump_queue("after modify buy ", &bq);
#endif
		} else if (tokens[0] == "PRINT") {
			print_orders("SELL:", sq.price_sorted);
			print_orders("BUY: ", bq.price_sorted);
		}
	}
#if DEBUG_DUMP
	dump_queue("final sell", &sq);
	dump_queue("final buy ", &bq);
#endif
	return 0;
}
