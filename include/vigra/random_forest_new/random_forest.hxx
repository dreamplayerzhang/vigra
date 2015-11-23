/************************************************************************/
/*                                                                      */
/*        Copyright 2014-2015 by Ullrich Koethe and Philip Schill       */
/*                                                                      */
/*    This file is part of the VIGRA computer vision library.           */
/*    The VIGRA Website is                                              */
/*        http://hci.iwr.uni-heidelberg.de/vigra/                       */
/*    Please direct questions, bug reports, and contributions to        */
/*        ullrich.koethe@iwr.uni-heidelberg.de    or                    */
/*        vigra@informatik.uni-hamburg.de                               */
/*                                                                      */
/*    Permission is hereby granted, free of charge, to any person       */
/*    obtaining a copy of this software and associated documentation    */
/*    files (the "Software"), to deal in the Software without           */
/*    restriction, including without limitation the rights to use,      */
/*    copy, modify, merge, publish, distribute, sublicense, and/or      */
/*    sell copies of the Software, and to permit persons to whom the    */
/*    Software is furnished to do so, subject to the following          */
/*    conditions:                                                       */
/*                                                                      */
/*    The above copyright notice and this permission notice shall be    */
/*    included in all copies or substantial portions of the             */
/*    Software.                                                         */
/*                                                                      */
/*    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND    */
/*    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES   */
/*    OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND          */
/*    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT       */
/*    HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,      */
/*    WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING      */
/*    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR     */
/*    OTHER DEALINGS IN THE SOFTWARE.                                   */
/*                                                                      */
/************************************************************************/
#ifndef VIGRA_RF_RANDOM_FOREST_HXX
#define VIGRA_RF_RANDOM_FOREST_HXX

#include <type_traits>
#include <thread>

#include "../multi_shape.hxx"
#include "../binary_forest.hxx"
#include "../threadpool.hxx"
#include "random_forest_common.hxx"



namespace vigra
{




template <typename FEATURES, typename LABELS, typename SPLITTESTS, typename ACCTYPE, ContainerTag CTag = MapTag>
class RandomForest
{
public:

    typedef FEATURES Features;
    typedef typename Features::value_type FeatureType;
    typedef LABELS Labels;
    typedef typename Labels::value_type LabelType;
    typedef SPLITTESTS SplitTests;
    typedef ACCTYPE ACC;
    typedef typename ACC::input_type AccInputType;
    typedef BinaryForest Graph;
    typedef Graph::Node Node;
    typedef std::vector<size_t> DistributionType;

    static ContainerTag const container_tag = CTag;

    // Default (empty) constructor.
    RandomForest();

    // Default constructor (copy all of the given stuff).
    RandomForest(
        Graph const & graph,
        PropertyMap<Node, SplitTests, CTag> const & split_tests,
        PropertyMap<Node, AccInputType, CTag> const & node_responses,
        ProblemSpecNew<LabelType> const & problem_spec
    );

    /// \brief Grow this forest by incorporating the other.
    void merge(
        RandomForest const & other
    );

    /// \brief Predict the given data and return the average number of split comparisons.
    /// \note labels should have the shape (features.shape()[0],).
    template <typename INDICES = std::vector<size_t> >
    double predict(
        FEATURES const & features,
        LABELS & labels,
        int n_threads = -1,
        INDICES const & tree_indices = std::vector<size_t>()
    ) const;

    /// \brief Predict the probabilities of the given data and return the average number of split comparisons.
    /// \note probs should have the shape (features.shape()[0], num_trees).
    template <typename PROBS, typename INDICES = std::vector<size_t> >
    double predict_proba(
        FEATURES const & features,
        PROBS & probs,
        int n_threads = -1,
        INDICES const & tree_indices = std::vector<size_t>()
    ) const;

    /// \brief For each data point in features, compute the corresponding leaf ids and return the average number of split comparisons.
    /// \note ids should have the shape (features.shape()[0], num_trees).
    template <typename IDS, typename INDICES = std::vector<size_t> >
    double leaf_ids(
        FEATURES const & features,
        IDS & ids,
        int n_threads = -1,
        INDICES const & tree_indices = std::vector<size_t>()
    ) const;

    /// \brief Return the number of nodes.
    size_t num_nodes() const
    {
        return graph_.numNodes();
    }

    /// \brief Return the number of trees.
    size_t num_trees() const
    {
        return graph_.numRoots();
    }

    /// \brief Return the number of classes.
    size_t num_classes() const
    {
        return problem_spec_.num_classes_;
    }

    /// \brief The graph structure.
    Graph graph_;

    /// \brief Contains a test for each internal node, that is used to determine whether given data goes to the left or the right child.
    PropertyMap<Node, SplitTests, CTag> split_tests_;

    /// \brief Contains the responses of each node (for example the most frequent label).
    PropertyMap<Node, AccInputType, CTag> node_responses_;

    /// \brief The specifications.
    ProblemSpecNew<LabelType> problem_spec_;

private:

    /// \brief Compute the leaf ids of the instances in [from, to).
    template <typename IDS, typename INDICES>
    double leaf_ids_impl(
        FEATURES const & features,
        IDS & ids,
        size_t from,
        size_t to,
        INDICES const & tree_indices
    ) const;

};

template <typename FEATURES, typename LABELS, typename SPLITTESTS, typename ACC, ContainerTag CTag>
RandomForest<FEATURES, LABELS, SPLITTESTS, ACC, CTag>::RandomForest()
    :
    graph_(),
    split_tests_(),
    node_responses_(),
    problem_spec_()
{}

template <typename FEATURES, typename LABELS, typename SPLITTESTS, typename ACC, ContainerTag CTag>
RandomForest<FEATURES, LABELS, SPLITTESTS, ACC, CTag>::RandomForest(
    Graph const & graph,
    PropertyMap<Node, SplitTests, CTag> const & split_tests,
    PropertyMap<Node, AccInputType, CTag> const & node_responses,
    ProblemSpecNew<LabelType> const & problem_spec
)   :
    graph_(graph),
    split_tests_(split_tests),
    node_responses_(node_responses),
    problem_spec_(problem_spec)
{}

template <typename FEATURES, typename LABELS, typename SPLITTESTS, typename ACC, ContainerTag CTag>
void RandomForest<FEATURES, LABELS, SPLITTESTS, ACC, CTag>::merge(
    RandomForest const & other
){
    vigra_precondition(problem_spec_ == other.problem_spec_,
                       "RandomForest::merge(): You cannot merge with different problem specs.");

    size_t const offset = num_nodes();
    graph_.merge(other.graph_);
    for (auto const & p : other.split_tests_)
    {
        split_tests_.insert(Node(p.first.id()+offset), p.second);
    }
    for (auto const & p : other.node_responses_)
    {
        node_responses_.insert(Node(p.first.id()+offset), p.second);
    }
}

template <typename FEATURES, typename LABELS, typename SPLITTESTS, typename ACC, ContainerTag CTag>
template <typename INDICES>
double RandomForest<FEATURES, LABELS, SPLITTESTS, ACC, CTag>::predict(
    FEATURES const & features,
    LABELS & labels,
    int n_threads,
    INDICES const & tree_indices
) const {
    vigra_precondition(features.shape()[0] == labels.shape()[0],
                       "RandomForest::predict(): Shape mismatch between features and labels.");
    vigra_precondition(features.shape()[1] == problem_spec_.num_features_,
                       "RandomForest::predict(): Number of features in prediction differs from training.");

    MultiArray<2, double> probs(Shape2(features.shape()[0], problem_spec_.num_classes_));
    double const average_split_counts = predict_proba(features, probs, n_threads, tree_indices);
    for (size_t i = 0; i < features.shape()[0]; ++i)
    {
        auto const sub_probs = probs.template bind<0>(i);
        auto it = std::max_element(sub_probs.begin(), sub_probs.end());
        size_t const label = std::distance(sub_probs.begin(), it);
        labels(i) = problem_spec_.distinct_classes_[label];
    }
    return average_split_counts;
}

template <typename FEATURES, typename LABELS, typename SPLITTESTS, typename ACC, ContainerTag CTag>
template <typename PROBS, typename INDICES>
double RandomForest<FEATURES, LABELS, SPLITTESTS, ACC, CTag>::predict_proba(
    FEATURES const & features,
    PROBS & probs,
    int n_threads,
    INDICES const & tree_indices
) const {
    vigra_precondition(features.shape()[0] == probs.shape()[0],
                       "RandomForest::predict_proba(): Shape mismatch between features and probabilities.");
    vigra_precondition(features.shape()[1] == problem_spec_.num_features_,
                       "RandomForest::predict_proba(): Number of features in prediction differs from training.");
    vigra_precondition(probs.shape()[1] == problem_spec_.num_classes_,
                       "RandomForest::predict_proba(): Number of labels in probabilities differs from training.");

    // Check the tree indices.
    std::set<size_t> actual_tree_indices(tree_indices.begin(), tree_indices.end());
    for (auto i : actual_tree_indices)
        vigra_precondition(i < graph_.numRoots(), "RandomForest::predict_proba(): Tree index out of range.");

    // By default, actual_tree_indices is empty. In that case we want to use all trees.
    if (actual_tree_indices.size() == 0)
        for (size_t i = 0; i < graph_.numRoots(); ++i)
            actual_tree_indices.insert(i);

    // Get the leaf ids.
    size_t const num_roots = graph_.numRoots();
    MultiArray<2, size_t> ids(Shape2(features.shape()[0], num_roots));
    double const average_split_counts = leaf_ids(features, ids, n_threads, actual_tree_indices);

    // Compute the probabilities.
    ACC acc;
    for (size_t i = 0; i < features.shape()[0]; ++i)
    {
        std::vector<AccInputType> tree_results;
        for (auto k : actual_tree_indices)
        {
            tree_results.push_back(node_responses_.at(Node(ids(i, k))));
        }
        auto sub_probs = probs.template bind<0>(i);
        acc(tree_results.begin(), tree_results.end(), sub_probs.begin());
    }
    return average_split_counts;
}

template <typename FEATURES, typename LABELS, typename SPLITTESTS, typename ACC, ContainerTag CTag>
template <typename IDS, typename INDICES>
double RandomForest<FEATURES, LABELS, SPLITTESTS, ACC, CTag>::leaf_ids(
    FEATURES const & features,
    IDS & ids,
    int n_threads,
    INDICES const & tree_indices
) const {
    vigra_precondition(features.shape()[0] == ids.shape()[0],
                       "RandomForest::leaf_ids(): Shape mismatch between features and probabilities.");
    vigra_precondition(features.shape()[1] == problem_spec_.num_features_,
                       "RandomForest::leaf_ids(): Number of features in prediction differs from training.");
    vigra_precondition(ids.shape()[1] == graph_.numRoots(),
                       "RandomForest::leaf_ids(): Leaf array has wrong shape.");

    // Check the tree indices.
    std::set<size_t> actual_tree_indices(tree_indices.begin(), tree_indices.end());
    for (auto i : actual_tree_indices)
        vigra_precondition(i < graph_.numRoots(), "RandomForest::leaf_ids(): Tree index out of range.");

    // By default, actual_tree_indices is empty. In that case we want to use all trees.
    if (actual_tree_indices.size() == 0)
        for (size_t i = 0; i < graph_.numRoots(); ++i)
            actual_tree_indices.insert(i);

    size_t const num_instances = features.shape()[0];
    if (n_threads == -1)
        n_threads = std::thread::hardware_concurrency();
    if (n_threads < 1)
        n_threads = 1;
    std::vector<double> split_comparisons(n_threads, 0.0);
    std::vector<size_t> indices(num_instances);
    std::iota(indices.begin(), indices.end(), 0);
    std::fill(ids.begin(), ids.end(), -1);
    parallel_foreach(
        n_threads,
        num_instances,
        indices.begin(),
        indices.end(),
        [this, &features, &ids, &split_comparisons, &actual_tree_indices](size_t thread_id, size_t i) {
            split_comparisons[thread_id] += this->leaf_ids_impl(features, ids, i, i+1, actual_tree_indices);
        }
    );

    double const sum_split_comparisons = std::accumulate(split_comparisons.begin(), split_comparisons.end(), 0.0);
    return sum_split_comparisons / features.shape()[0];
}

template <typename FEATURES, typename LABELS, typename SPLITTESTS, typename ACC, ContainerTag CTag>
template <typename IDS, typename INDICES>
double RandomForest<FEATURES, LABELS, SPLITTESTS, ACC, CTag>::leaf_ids_impl(
    FEATURES const & features,
    IDS & ids,
    size_t from,
    size_t to,
    INDICES const & tree_indices
) const {
    vigra_precondition(features.shape()[0] == ids.shape()[0],
                       "RandomForest::leaf_ids_impl(): Shape mismatch between features and labels.");
    vigra_precondition(features.shape()[1] == problem_spec_.num_features_,
                       "RandomForest::leaf_ids_impl(): Number of Features in prediction differs from training.");
    vigra_precondition(from >= 0 && from <= to && to <= features.shape()[0],
                       "RandomForest::leaf_ids_impl(): Indices out of range.");
    vigra_precondition(ids.shape()[1] == graph_.numRoots(),
                       "RandomForest::leaf_ids_impl(): Leaf array has wrong shape.");

    double split_comparisons = 0.0;
    for (size_t i = from; i < to; ++i)
    {
        auto const sub_features = features.template bind<0>(i);
        for (auto k : tree_indices)
        {
            Node node = graph_.getRoot(k);
            while (graph_.outDegree(node) > 0)
            {
                size_t const child_index = split_tests_.at(node)(sub_features);
                node = graph_.getChild(node, child_index);
                split_comparisons += 1.0;
            }
            ids(i, k) = node.id();
        }
    }
    return split_comparisons;
}



} // namespace vigra

#endif