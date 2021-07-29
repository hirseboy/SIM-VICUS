#ifndef DIFFER_H
#define DIFFER_H

#include <vector>
#include <string>


namespace IBK {

/*! See https://florian.github.io/diffing */
template <typename T>
class Differ {
public:
	/*! default constructor */
	Differ() = default;

	/*! constructor */
	Differ(const std::vector<T> &obj1, const std::vector<T> &obj2);

	/*! calculates the longest common subsequence (LCS) as a matrix */
	void calculateLCS();

	/*! compares obj1 vs obj2. The result is stored in m_resultObj, conataining the "merged" vector of both objects
		which conatins each element of both vectors. The operation which needs to be done to merge obj2 into obj1 is
		stored in m_resultOperation.
	*/
	void diff();

	/*! the length of the longest common subsequence */
	unsigned int lcsLength();

	bool									m_lcsCalculated = false;

	std::vector<T>							m_obj1;

	std::vector<T>							m_obj2;

	std::vector<std::vector<unsigned int> > m_lcs;

	std::vector<T >							m_resultObj;

	std::vector<std::string>				m_resultOperation;

};

} // namespace IBK


#endif // DIFFER_H
