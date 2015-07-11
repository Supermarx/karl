#pragma once

#include <deque>

#include <karl/util/similarity_matrix.hpp>

namespace supermarx
{

template<typename SIMILARITY_T>
class hungarian_fast
{
public:
	typedef std::pair<size_t, size_t> coord_t;
	typedef coord_t matching_e;
	typedef std::vector<matching_e> matching_t;

private:
	using cost_e = typename similarity_matrix<SIMILARITY_T>::similarity;
	typedef matrix<cost_e> cost_t;

	const size_t orig_width, orig_height, n;

	cost_t cost;

	std::vector<boost::optional<size_t>> xy, yx, aug_path;
	std::vector<bool> S, T;
	std::vector<cost_e> slack;
	std::vector<size_t> slackx;

public:
	hungarian_fast(const similarity_matrix<SIMILARITY_T>& similarity)
	: orig_width(similarity.width())
	, orig_height(similarity.height())
	, n(std::max(orig_width, orig_height))
	, cost(n, n, 0)
	, xy(n, boost::none)
	, yx(n, boost::none)
	, aug_path()
	, S()
	, T()
	, slack()
	, slackx()
	{
		//Find some initial feasible vertex labeling and some initial matching
		for(size_t x = 0; x < n; x++)
		{
			cost_e max = std::numeric_limits<cost_e>::min();

			if(x < orig_height)
				for(size_t y = 0; y < orig_width; y++)
					max = std::max(max, similarity(x, y));

			for(size_t y = 0; y < n; y++)
				cost(x, y) = max;

			if(x < orig_height)
				for(size_t y = 0; y < orig_width; y++)
					cost(x, y) -= similarity(x, y);
		}
	}

	matching_t produce()
	{
		matching_t result;

		find_matching();

		for(size_t y = 0; y < orig_height; y++)
			if(xy[y].get() < orig_width)
				result.emplace_back(y, xy[y].get());

		return result;
	}

private:
	void compute_slack(const size_t x)
	{
		for(size_t y = 0; y < n; y++)
		{
			if(cost(x, y) >= slack[y])
				continue;

			slack[y] = cost(x, y);
			slackx[y] = x;
		}
	}

	void assign(const size_t x, const size_t y)
	{
		xy[x] = y;
		yx[y] = x;
	}

	void add_to_path(const size_t x, const size_t prevx)
	{
		aug_path[x] = prevx;
		S[x] = true;
		compute_slack(x);
	}

	void update_labels()
	{
		cost_e delta = std::numeric_limits<cost_e>::max();
		for(size_t i = 0; i < n; i++)
			if(!T[i])
				delta = std::min(delta, slack[i]);

		for(size_t i = 0; i < n; i++)
		{
			if(S[i])
				for(size_t y = 0; y < n; y++)
					cost(i, y) -= delta;

			if(T[i])
				for(size_t x = 0; x < n; x++)
					cost(x, i) += delta;
			else
				slack[i] -= delta;
		}
	}

	void flip_edges(const coord_t start)
	{
		//We did not find an augmenting path
		//Flip the edges along the augmenting path
		//This means we will add one more item to our matching
		for(
			boost::optional<size_t> cx(start.first), cy(start.second), ty(boost::none);
			cx;
			cx = aug_path[cx.get()], cy = ty
		)
		{
			ty = xy[cx.get()];
			assign(cx.get(), cy.get());
		}
	}

	bool build_path_bfs(std::deque<size_t>& q, coord_t& start)
	{
		while(q.size() > 0)
		{
			const size_t x = q.front();
			q.pop_front();

			for(size_t y = 0; y < n; y++)
				if(!T[y] && cost(x, y) == 0)
				{
					if(!yx[y])
					{
						start = std::make_pair(x, y);
						return true;
					}
					else
					{
						const size_t yxy = yx[y].get();
						T[y] = true;

						q.push_back(yxy);
						add_to_path(yxy, x);
					}
				}
		}

		return false;
	}

	bool enhance_path(std::deque<size_t>& q, coord_t& start)
	{
		for(size_t y = 0; y < n; y++)
			if(!T[y] && slack[y] == 0)
			{
				if(!yx[y]) //Exposed vertex found; augmenting path exists
				{
					start = std::make_pair(slackx[y], y);
					return true;
				}
				else
				{
					const size_t yxy = yx[y].get();
					T[y] = true;

					if(S[yxy])
						continue;

					q.push_back(yxy);
					add_to_path(yxy, slackx[y]);
				}
			}

		return false;
	}

	void find_matching()
	{
		for(size_t match_round = 0; match_round < n; match_round++)
		{
			std::deque<size_t> q; //Set of unmatched x's

			S.assign(n, false);
			T.assign(n, false);

			slack.assign(n, std::numeric_limits<cost_e>::max());
			slackx.resize(n);
			aug_path.assign(n, boost::none);

			//Find the first element to start the bfs for
			for(size_t x = 0; x < n; x++)
			{
				if(xy[x]) //If x is matched, skip
					continue;

				q.push_back(x);
				S[x] = true;
				compute_slack(x);
				break;
			}

			coord_t start(0, 0);

			do
			{
				if(build_path_bfs(q, start))
					break;

				update_labels();
				q.clear();

			} while(!enhance_path(q, start));

			flip_edges(start);
		}
	}
};

}
